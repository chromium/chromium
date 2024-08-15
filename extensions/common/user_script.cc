// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/user_script.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/switches.h"

namespace {

// The length of all internally appended prefixes for a UserScript's ID.
const size_t kIDPrefixLength = 4;

// This cannot be a plain int or int64_t because we need to generate unique IDs
// from multiple threads.
base::AtomicSequenceNumber g_user_script_id_generator;

bool UrlMatchesGlobs(const std::vector<std::string>* globs,
                     const GURL& url) {
  for (const auto& glob : *globs) {
    if (base::MatchPattern(url.spec(), glob)) {
      return true;
    }
  }

  return false;
}

constexpr const char* kAllPrefixes[] = {
    extensions::UserScript::kManifestContentScriptPrefix,
    extensions::UserScript::kDynamicContentScriptPrefix,
    extensions::UserScript::kDynamicUserScriptPrefix,
};

constexpr bool ValidatePrefixes() {
  for (const char* prefix : kAllPrefixes) {
    if (prefix[0] != extensions::UserScript::kReservedScriptIDPrefix ||
        std::char_traits<char>::length(prefix) != kIDPrefixLength) {
      return false;
    }
  }
  return true;
}

static_assert(ValidatePrefixes(), "At least one prefix is invalid.");

}  // namespace

namespace extensions {

// The bitmask for valid user script injectable schemes used by URLPattern.
enum {
  kValidUserScriptSchemes = URLPattern::SCHEME_CHROMEUI |
                            URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
                            URLPattern::SCHEME_FILE | URLPattern::SCHEME_FTP |
                            URLPattern::SCHEME_UUID_IN_PACKAGE
};

// static
std::string UserScript::GenerateUserScriptID() {
  // This could just as easily use a GUID. The actual value of the id is not
  // important as long a unique id is generated for each UserScript.
  return kManifestContentScriptPrefix +
         base::NumberToString(g_user_script_id_generator.GetNext());
}

// static
std::string UserScript::TrimPrefixFromScriptID(const std::string& script_id) {
  return script_id.substr(kIDPrefixLength);
}

// static
UserScript::Source UserScript::GetSourceForScriptID(
    const std::string& script_id) {
  if (base::StartsWith(script_id, kManifestContentScriptPrefix)) {
    return Source::kStaticContentScript;
  }

  if (base::StartsWith(script_id, kDynamicContentScriptPrefix)) {
    return Source::kDynamicContentScript;
  }

  if (base::StartsWith(script_id, kDynamicUserScriptPrefix)) {
    return Source::kDynamicUserScript;
  }

  // TODO(crbug.com/40927913): Handle gracefully when a new source is handed,
  // specially when user has different Chrome versions.
  NOTREACHED();
}

// static
bool UserScript::IsURLUserScript(const GURL& url,
                                 const std::string& mime_type) {
  return base::EndsWith(url.ExtractFileName(), kFileExtension,
                        base::CompareCase::INSENSITIVE_ASCII) &&
         mime_type != "text/html";
}

// static
int UserScript::ValidUserScriptSchemes(bool can_execute_script_everywhere) {
  if (can_execute_script_everywhere) {
    return URLPattern::SCHEME_ALL;
  }
  int valid_schemes = kValidUserScriptSchemes;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExtensionsOnChromeURLs)) {
    valid_schemes &= ~URLPattern::SCHEME_CHROMEUI;
  }
  return valid_schemes;
}

UserScript::Content::Content(Source source,
                             const base::FilePath& extension_root,
                             const base::FilePath& relative_path,
                             const GURL& url)
    : source_(source),
      extension_root_(extension_root),
      relative_path_(relative_path),
      url_(url) {}

// static
std::unique_ptr<UserScript::Content> UserScript::Content::CreateFile(
    const base::FilePath& extension_root,
    const base::FilePath& relative_path,
    const GURL& url) {
  return base::WrapUnique(new UserScript::Content(Source::kFile, extension_root,
                                                  relative_path, url));
}

// static
std::unique_ptr<UserScript::Content> UserScript::Content::CreateInlineCode(
    const GURL& url) {
  return base::WrapUnique(new UserScript::Content(
      Source::kInlineCode, base::FilePath(), base::FilePath(), url));
}

UserScript::Content::Content() = default;

// File content is not copied.
UserScript::Content::Content(const Content& other)
    : source_(other.source_),
      extension_root_(other.extension_root_),
      relative_path_(other.relative_path_),
      url_(other.url_) {}

UserScript::Content::~Content() = default;

UserScript::UserScript() = default;
UserScript::~UserScript() = default;

// static.
std::unique_ptr<UserScript> UserScript::CopyMetadataFrom(
    const UserScript& other) {
  std::unique_ptr<UserScript> script(new UserScript());
  script->run_location_ = other.run_location_;
  script->name_space_ = other.name_space_;
  script->name_ = other.name_;
  script->description_ = other.description_;
  script->version_ = other.version_;
  script->globs_ = other.globs_;
  script->exclude_globs_ = other.exclude_globs_;
  script->url_set_ = other.url_set_.Clone();
  script->exclude_url_set_ = other.exclude_url_set_.Clone();

  // Note: Content is not copied.
  for (const std::unique_ptr<Content>& file : other.js_scripts()) {
    std::unique_ptr<Content> file_copy(new Content(*file));
    script->js_scripts_.push_back(std::move(file_copy));
  }
  for (const std::unique_ptr<Content>& file : other.css_scripts()) {
    std::unique_ptr<Content> file_copy(new Content(*file));
    script->css_scripts_.push_back(std::move(file_copy));
  }
  script->host_id_ = other.host_id_;
  script->consumer_instance_type_ = other.consumer_instance_type_;
  script->user_script_id_ = other.user_script_id_;
  script->emulate_greasemonkey_ = other.emulate_greasemonkey_;
  script->match_all_frames_ = other.match_all_frames_;
  script->match_origin_as_fallback_ = other.match_origin_as_fallback_;
  script->incognito_enabled_ = other.incognito_enabled_;
  script->execution_world_ = other.execution_world_;
  script->world_id_ = other.world_id_;

  return script;
}

void UserScript::add_url_pattern(const URLPattern& pattern) {
  url_set_.AddPattern(pattern);
}

void UserScript::add_exclude_url_pattern(const URLPattern& pattern) {
  exclude_url_set_.AddPattern(pattern);
}

std::string UserScript::GetIDWithoutPrefix() const {
  return TrimPrefixFromScriptID(user_script_id_);
}

UserScript::Source UserScript::GetSource() const {
  if (host_id_.type == mojom::HostID::HostType::kWebUi) {
    return Source::kWebUIScript;
  }

  return GetSourceForScriptID(user_script_id_);
}

bool UserScript::MatchesURL(const GURL& url) const {
  if (!exclude_url_set_.is_empty() && exclude_url_set_.MatchesURL(url)) {
    return false;
  }

  if (!exclude_globs_.empty() && UrlMatchesGlobs(&exclude_globs_, url)) {
    return false;
  }

  // User scripts need to match url patterns OR include globs, if present.
  if (GetSource() == UserScript::Source::kDynamicUserScript) {
    return (url_set_.MatchesURL(url) || UrlMatchesGlobs(&globs_, url));
  }

  // Other scripts need to match url patterns AND include globs, if present.
  return (url_set_.is_empty() || url_set_.MatchesURL(url)) &&
         (globs_.empty() || UrlMatchesGlobs(&globs_, url));
}

bool UserScript::MatchesDocument(const GURL& effective_document_url,
                                 bool is_subframe) const {
  if (is_subframe && !match_all_frames()) {
    return false;
  }

  return MatchesURL(effective_document_url);
}

void UserScript::Content::Pickle(base::Pickle* pickle) const {
  pickle->WriteString(url_.spec());
  // Do not write path. It's not needed in the renderer.
  // Do not write content. It will be serialized by other means.
}

void UserScript::Content::Unpickle(const base::Pickle& pickle,
                                   base::PickleIterator* iter) {
  // Read the url from the pickle.
  std::string url;
  CHECK(iter->ReadString(&url));
  set_url(GURL(url));
}

void UserScript::Pickle(base::Pickle* pickle) const {
  // Write the simple types to the pickle.
  pickle->WriteInt(static_cast<int>(run_location()));
  pickle->WriteString(user_script_id_);
  pickle->WriteBool(emulate_greasemonkey());
  pickle->WriteBool(match_all_frames());
  pickle->WriteInt(static_cast<int>(match_origin_as_fallback()));
  pickle->WriteBool(is_incognito_enabled());
  pickle->WriteInt(static_cast<int>(execution_world()));
  // Pickling doesn't really have support for optionals. If there's no world ID
  // specified, simply pass an "_".
  pickle->WriteString(world_id().value_or("_"));

  PickleHostID(pickle, host_id_);
  pickle->WriteInt(consumer_instance_type());
  PickleGlobs(pickle, globs_);
  PickleGlobs(pickle, exclude_globs_);
  PickleURLPatternSet(pickle, url_set_);
  PickleURLPatternSet(pickle, exclude_url_set_);
  PickleScripts(pickle, js_scripts_);
  PickleScripts(pickle, css_scripts_);
}

void UserScript::PickleGlobs(base::Pickle* pickle,
                             const std::vector<std::string>& globs) const {
  pickle->WriteUInt32(globs.size());
  for (auto glob = globs.cbegin(); glob != globs.cend(); ++glob) {
    pickle->WriteString(*glob);
  }
}

void UserScript::PickleHostID(base::Pickle* pickle,
                              const mojom::HostID& host_id) const {
  pickle->WriteInt(static_cast<int>(host_id.type));
  pickle->WriteString(host_id.id);
}

void UserScript::PickleURLPatternSet(base::Pickle* pickle,
                                     const URLPatternSet& pattern_list) const {
  pickle->WriteUInt32(pattern_list.patterns().size());
  for (auto pattern = pattern_list.begin(); pattern != pattern_list.end();
       ++pattern) {
    pickle->WriteInt(pattern->valid_schemes());
    pickle->WriteString(pattern->GetAsString());
  }
}

void UserScript::PickleScripts(base::Pickle* pickle,
                               const ContentList& scripts) const {
  pickle->WriteUInt32(scripts.size());
  for (const std::unique_ptr<Content>& file : scripts) {
    file->Pickle(pickle);
  }
}

void UserScript::Unpickle(const base::Pickle& pickle,
                          base::PickleIterator* iter) {
  // Read the run location.
  int run_location = 0;
  CHECK(iter->ReadInt(&run_location));
  CHECK(run_location >= static_cast<int>(mojom::RunLocation::kUndefined) &&
        run_location <= static_cast<int>(mojom::RunLocation::kMaxValue));
  run_location_ = static_cast<mojom::RunLocation>(run_location);

  CHECK(iter->ReadString(&user_script_id_));
  CHECK(iter->ReadBool(&emulate_greasemonkey_));
  CHECK(iter->ReadBool(&match_all_frames_));
  int match_origin_as_fallback_int = 0;
  CHECK(iter->ReadInt(&match_origin_as_fallback_int));
  match_origin_as_fallback_ =
      static_cast<MatchOriginAsFallbackBehavior>(match_origin_as_fallback_int);
  CHECK(iter->ReadBool(&incognito_enabled_));

  // Read the execution world.
  int execution_world = 0;
  CHECK(iter->ReadInt(&execution_world));
  CHECK(execution_world >= static_cast<int>(mojom::ExecutionWorld::kIsolated) &&
        execution_world <= static_cast<int>(mojom::ExecutionWorld::kMaxValue));
  execution_world_ = static_cast<mojom::ExecutionWorld>(execution_world);

  std::string world_id_str;
  CHECK(iter->ReadString(&world_id_str));
  if (world_id_str != "_") {
    // Pickling doesn't support optionals. We pass an "_" as a placeholder if
    // there was no world ID specified on the original UserScript.
    world_id_ = world_id_str;
  }

  UnpickleHostID(pickle, iter, &host_id_);

  int consumer_instance_type = 0;
  CHECK(iter->ReadInt(&consumer_instance_type));
  consumer_instance_type_ =
      static_cast<ConsumerInstanceType>(consumer_instance_type);

  UnpickleGlobs(pickle, iter, &globs_);
  UnpickleGlobs(pickle, iter, &exclude_globs_);
  UnpickleURLPatternSet(pickle, iter, &url_set_);
  UnpickleURLPatternSet(pickle, iter, &exclude_url_set_);
  UnpickleScripts(pickle, iter, &js_scripts_);
  UnpickleScripts(pickle, iter, &css_scripts_);
}

void UserScript::UnpickleGlobs(const base::Pickle& pickle,
                               base::PickleIterator* iter,
                               std::vector<std::string>* globs) {
  uint32_t num_globs = 0;
  CHECK(iter->ReadUInt32(&num_globs));
  globs->clear();
  for (uint32_t i = 0; i < num_globs; ++i) {
    std::string glob;
    CHECK(iter->ReadString(&glob));
    globs->push_back(glob);
  }
}

void UserScript::UnpickleHostID(const base::Pickle& pickle,
                                base::PickleIterator* iter,
                                mojom::HostID* host_id) {
  int type = 0;
  std::string id;
  CHECK(iter->ReadInt(&type));
  CHECK(iter->ReadString(&id));
  *host_id = mojom::HostID(static_cast<mojom::HostID::HostType>(type), id);
}

void UserScript::UnpickleURLPatternSet(const base::Pickle& pickle,
                                       base::PickleIterator* iter,
                                       URLPatternSet* pattern_list) {
  uint32_t num_patterns = 0;
  CHECK(iter->ReadUInt32(&num_patterns));

  pattern_list->ClearPatterns();
  for (uint32_t i = 0; i < num_patterns; ++i) {
    int valid_schemes;
    CHECK(iter->ReadInt(&valid_schemes));

    std::string pattern_str;
    CHECK(iter->ReadString(&pattern_str));

    URLPattern pattern(kValidUserScriptSchemes);
    URLPattern::ParseResult result = pattern.Parse(pattern_str);
    CHECK(URLPattern::ParseResult::kSuccess == result)
        << URLPattern::GetParseResultString(result) << " "
        << pattern_str.c_str();

    pattern.SetValidSchemes(valid_schemes);
    pattern_list->AddPattern(pattern);
  }
}

void UserScript::UnpickleScripts(const base::Pickle& pickle,
                                 base::PickleIterator* iter,
                                 ContentList* scripts) {
  uint32_t num_files = 0;
  CHECK(iter->ReadUInt32(&num_files));
  scripts->clear();
  for (uint32_t i = 0; i < num_files; ++i) {
    std::unique_ptr<Content> file(new Content());
    file->Unpickle(pickle, iter);
    scripts->push_back(std::move(file));
  }
}

}  // namespace extensions
