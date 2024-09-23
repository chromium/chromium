// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_set.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/renderer/extension_injection_host.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_injector.h"
#include "extensions/renderer/web_ui_injection_host.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// These two strings are injected before and after the Greasemonkey API and
// user script to wrap it in an anonymous scope.
const char kUserScriptHead[] = "(function (unsafeWindow) {\n";
const char kUserScriptTail[] = "\n})(window);";
// Maximum number of total content scripts we allow (across all extensions).
// The limit exists to diagnose https://crbug.com/723381. The number is
// arbitrarily chosen.
// TODO(lazyboy): Remove when the bug is fixed.
const uint32_t kNumScriptsArbitraryMax = 100000u;

GURL GetDocumentUrlForFrame(blink::WebLocalFrame* frame) {
  GURL data_source_url = ScriptContext::GetDocumentLoaderURLForFrame(frame);
  if (!data_source_url.is_empty() && frame->IsViewSourceModeEnabled()) {
    data_source_url = GURL(content::kViewSourceScheme + std::string(":") +
                           data_source_url.spec());
  }

  return data_source_url;
}

}  // namespace

UserScriptSet::UserScriptSet(mojom::HostID host_id)
    : host_id_(std::move(host_id)) {}

UserScriptSet::~UserScriptSet() {
  for (auto& observer : observers_)
    observer.OnUserScriptSetDestroyed();
}

void UserScriptSet::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptSet::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserScriptSet::GetInjections(
    std::vector<std::unique_ptr<ScriptInjection>>* injections,
    content::RenderFrame* render_frame,
    int tab_id,
    mojom::RunLocation run_location,
    bool log_activity) {
  GURL document_url = GetDocumentUrlForFrame(render_frame->GetWebFrame());
  for (const std::unique_ptr<UserScript>& script : scripts_) {
    std::unique_ptr<ScriptInjection> injection = GetInjectionForScript(
        script.get(), render_frame, tab_id, run_location, document_url,
        false /* is_declarative */, log_activity);
    if (injection.get())
      injections->push_back(std::move(injection));
  }
}

bool UserScriptSet::UpdateUserScripts(
    base::ReadOnlySharedMemoryRegion shared_memory) {
  bool only_inject_incognito =
      ExtensionsRendererClient::Get()->IsIncognitoProcess();

  // Create the shared memory mapping.
  shared_memory_mapping_ = shared_memory.Map();
  if (!shared_memory.IsValid())
    return false;

  // First get the size of the memory block.
  const base::Pickle::Header* pickle_header =
      shared_memory_mapping_.GetMemoryAs<base::Pickle::Header>();
  if (!pickle_header)
    return false;

  // Now read in the rest of the block.
  size_t pickle_size =
      sizeof(base::Pickle::Header) + pickle_header->payload_size;

  // Unpickle scripts.
  uint32_t num_scripts = 0;
  auto memory = shared_memory_mapping_.GetMemoryAsSpan<uint8_t>(pickle_size);
  if (!memory.size())
    return false;

  base::Pickle pickle = base::Pickle::WithUnownedBuffer(memory);
  base::PickleIterator iter(pickle);
  base::debug::Alias(&pickle_size);
  CHECK(iter.ReadUInt32(&num_scripts));

  // Sometimes the shared memory contents seem to be corrupted
  // (https://crbug.com/723381). Set an arbitrary max limit to the number of
  // scripts so that we don't add OOM noise to crash reports.
  CHECK_LT(num_scripts, kNumScriptsArbitraryMax);

  scripts_.clear();
  script_sources_.clear();
  scripts_.reserve(num_scripts);
  for (uint32_t i = 0; i < num_scripts; ++i) {
    std::unique_ptr<UserScript> script(new UserScript());
    script->Unpickle(pickle, &iter);

    // Note that this is a pointer into shared memory. We don't own it. It
    // gets cleared up when the last renderer or browser process drops their
    // reference to the shared memory.
    for (const auto& js_script : script->js_scripts()) {
      const char* body = nullptr;
      size_t body_length = 0;
      CHECK(iter.ReadData(&body, &body_length));
      js_script->set_external_content(std::string_view(body, body_length));
    }
    for (const auto& css_script : script->css_scripts()) {
      const char* body = nullptr;
      size_t body_length = 0;
      CHECK(iter.ReadData(&body, &body_length));
      css_script->set_external_content(std::string_view(body, body_length));
    }

    if (only_inject_incognito && !script->is_incognito_enabled())
      continue;  // This script shouldn't run in an incognito tab.

    scripts_.push_back(std::move(script));
  }

  for (auto& observer : observers_)
    observer.OnUserScriptsUpdated();
  return true;
}

void UserScriptSet::ClearUserScripts() {
  scripts_.clear();
  script_sources_.clear();
  for (auto& observer : observers_)
    observer.OnUserScriptsUpdated();
}

std::unique_ptr<ScriptInjection> UserScriptSet::GetDeclarativeScriptInjection(
    const std::string& script_id,
    content::RenderFrame* render_frame,
    int tab_id,
    mojom::RunLocation run_location,
    const GURL& document_url,
    bool log_activity) {
  for (const std::unique_ptr<UserScript>& script : scripts_) {
    if (script->id() == script_id) {
      return GetInjectionForScript(script.get(), render_frame, tab_id,
                                   run_location, document_url,
                                   true /* is_declarative */, log_activity);
    }
  }
  return nullptr;
}

std::unique_ptr<ScriptInjection> UserScriptSet::GetInjectionForScript(
    const UserScript* script,
    content::RenderFrame* render_frame,
    int tab_id,
    mojom::RunLocation run_location,
    const GURL& document_url,
    bool is_declarative,
    bool log_activity) {
  std::unique_ptr<ScriptInjection> injection;
  std::unique_ptr<const InjectionHost> injection_host;
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();

  switch (host_id_.type) {
    case mojom::HostID::HostType::kExtensions:
      injection_host = ExtensionInjectionHost::Create(host_id_.id);
      if (!injection_host) {
        return injection;
      }
      break;
    case mojom::HostID::HostType::kControlledFrameEmbedder:
    case mojom::HostID::HostType::kWebUi:
      injection_host = std::make_unique<WebUIInjectionHost>(host_id_);
      break;
  }

  GURL effective_document_url =
      ScriptContext::GetEffectiveDocumentURLForInjection(
          web_frame, document_url, script->match_origin_as_fallback());

  bool is_subframe = !web_frame->IsOutermostMainFrame();
  if (!script->MatchesDocument(effective_document_url, is_subframe))
    return injection;

  // Extension dynamic scripts are treated as declarative scripts and should use
  // host permissions instead of scriptable hosts to determine if they should be
  // injected into a frame.
  bool is_extension_dynamic_script =
      (host_id_.type == mojom::HostID::HostType::kExtensions) &&
      (script->GetSource() == UserScript::Source::kDynamicContentScript ||
       script->GetSource() == UserScript::Source::kDynamicUserScript);
  std::unique_ptr<ScriptInjector> injector(new UserScriptInjector(
      script, this, is_declarative || is_extension_dynamic_script));

  if (injector->CanExecuteOnFrame(injection_host.get(), web_frame, tab_id) ==
      PermissionsData::PageAccess::kDenied) {
    return injection;
  }

  bool inject_css = !script->css_scripts().empty() &&
                    run_location == mojom::RunLocation::kDocumentStart;
  bool inject_js =
      !script->js_scripts().empty() && script->run_location() == run_location;
  if (inject_css || inject_js) {
    injection = std::make_unique<ScriptInjection>(
        std::move(injector), render_frame, std::move(injection_host),
        run_location, log_activity);
  }
  return injection;
}

blink::WebString UserScriptSet::GetJsSource(const UserScript::Content& file,
                                            bool emulate_greasemonkey) {
  const GURL& url = file.url();
  auto iter = script_sources_.find(url);
  if (iter != script_sources_.end())
    return iter->second;

  std::string_view script_content = file.GetContent();
  blink::WebString source;
  if (emulate_greasemonkey) {
    // We add this dumb function wrapper for user scripts to emulate what
    // Greasemonkey does. |script_content| becomes:
    // concat(kUserScriptHead, script_content, kUserScriptTail).
    std::string content =
        base::StrCat({kUserScriptHead, script_content, kUserScriptTail});
    source = blink::WebString::FromUTF8(content);
  } else {
    source = blink::WebString::FromUTF8(script_content);
  }
  script_sources_[url] = source;
  return source;
}

blink::WebString UserScriptSet::GetCssSource(const UserScript::Content& file) {
  const GURL& url = file.url();
  auto iter = script_sources_.find(url);
  if (iter != script_sources_.end())
    return iter->second;

  std::string_view script_content = file.GetContent();
  return script_sources_
      .insert(std::make_pair(url, blink::WebString::FromUTF8(script_content)))
      .first->second;
}

}  // namespace extensions
