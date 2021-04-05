// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_script_tracker.h"

#include <algorithm>
#include <map>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace {

// Crrently we track content script injection per-RenderFrameHost (using
// GlobalFrameRoutingId as the key of the map below).
// RenderDocumentHostUserData is not used because of a race between content
// script injection, ReadyToCommit and DidCommit (see the
// ContentScriptTrackerBrowserTest.ProgrammaticInjectionRacingWithDidCommit test
// for more details).
//
// TODO(lukasza): Once RenderDocumentHost project ships, we should switch to
// per-document tracking.
using FrameToExtensionIdSet =
    std::map<content::GlobalFrameRoutingId, ExtensionIdSet>;

FrameToExtensionIdSet& GetFrameToExtensionIdSet() {
  static base::NoDestructor<FrameToExtensionIdSet> frame_to_extension_id_set;
  return *frame_to_extension_id_set;
}

ExtensionIdSet& GetOrCreateExtensionIdSet(content::RenderFrameHost* frame) {
  FrameToExtensionIdSet& frame_to_extension_id_set = GetFrameToExtensionIdSet();
  return frame_to_extension_id_set[frame->GetGlobalFrameRoutingId()];
}

// If `match_about_blank` is true, then traverses parent/opener chain until the
// first non-about-scheme document and returns its url.  Otherwise, simply
// returns `document_url`.
//
// This function approximates
// ScriptContext::GetEffectiveDocumentURLForInjection() from the renderer side.
// Unlike the renderer code, this just iterates up frame tree, and doesn't look
// at the effective or precursor origin of the frame. This is okay, because our
// only caller (DoesContentScriptMatch()) expects false positives.
GURL GetEffectiveDocumentURL(content::RenderFrameHost* frame,
                             const GURL& document_url,
                             bool match_about_blank) {
  base::flat_set<content::RenderFrameHost*> already_visited_frames;

  // Common scenario. If `match_about_blank` is false (as is the case in most
  // extensions), or if the frame is not an about:-page, just return
  // `document_url` (supposedly the URL of the frame).
  if (!match_about_blank || !document_url.SchemeIs(url::kAboutScheme))
    return document_url;

  // Non-sandboxed about:blank and about:srcdoc pages inherit their security
  // origin from their parent frame/window. So, traverse the frame/window
  // hierarchy to find the closest non-about:-page and return its URL.
  //
  // TODO(https://crbug.com/1186321): The assumption above is incorrect -
  // about:blank frames inherit their origin from the initiator of the
  // navigation (which might not be the parent and/or the opener).
  content::RenderFrameHost* found_frame = frame;
  do {
    DCHECK(found_frame);
    already_visited_frames.insert(found_frame);

    // The loop should only execute (and consider the parent chain) if the
    // currently considered frame has about: scheme.
    DCHECK(match_about_blank);
    DCHECK(
        ((found_frame == frame) && document_url.SchemeIs(url::kAboutScheme)) ||
        (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme)));

    // Attempt to find `next_candidate` - either a parent of opener of
    // `found_frame`.
    content::RenderFrameHost* next_candidate = found_frame->GetParent();
    if (!next_candidate) {
      next_candidate =
          content::WebContents::FromRenderFrameHost(found_frame)->GetOpener();
    }
    if (!next_candidate ||
        base::Contains(already_visited_frames, next_candidate)) {
      break;
    }

    found_frame = next_candidate;
  } while (found_frame->GetLastCommittedURL().SchemeIs(url::kAboutScheme));

  if (found_frame == frame)
    return document_url;  // Not committed yet at ReadyToCommitNavigation time.
  return found_frame->GetLastCommittedURL();
}

// If `user_script` will inject JavaScript content script into the target of
// `navigation`, then DoesContentScriptMatch returns true.  Otherwise it may
// return either true or false.  Note that this function ignores CSS content
// scripts.
//
// This function approximates a subset of checks from
// UserScriptSet::GetInjectionForScript (which runs in the renderer process).
// Unlike the renderer version, the code below doesn't consider ability to
// create an injection host or the results of ScriptInjector::CanExecuteOnFrame.
// Additionally the `effective_url` calculations are also only an approximation.
// This is okay, because the top-level doc comment for ContentScriptTracker
// documents that false positives are expected and why they are okay.
bool DoesContentScriptMatch(const UserScript& user_script,
                            content::RenderFrameHost* frame,
                            const GURL& url) {
  // ContentScriptTracker only needs to track Javascript content scripts (e.g.
  // doesn't track CSS-only injections).
  if (user_script.js_scripts().empty())
    return false;

  // TODO(devlin): Update GetEffectiveDocumentURL() to take a
  // MatchOriginAsFallbackBehavior.
  bool match_about_blank = false;
  switch (user_script.match_origin_as_fallback()) {
    case MatchOriginAsFallbackBehavior::kAlways:
    case MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree:
      match_about_blank = true;
      break;
    case MatchOriginAsFallbackBehavior::kNever:
      break;  // `false` is correct for `match_about_blank`.
  }
  GURL effective_url = GetEffectiveDocumentURL(frame, url, match_about_blank);
  bool is_subframe = frame->GetParent();
  return user_script.MatchesDocument(effective_url, is_subframe);
}

void HandleProgrammaticContentScriptInjection(
    base::PassKey<ContentScriptTracker> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ExtensionIdSet& content_scripts_for_frame = GetOrCreateExtensionIdSet(frame);
  content_scripts_for_frame.insert(extension.id());

  URLLoaderFactoryManager::WillProgrammaticallyInjectContentScript(
      pass_key, frame, extension);
}

// If `extension`'s manifest declares that it may inject JavaScript content
// script into the `frame` / `url`, then DoContentScriptsMatch returns true.
// Otherwise it may return either true or false.
//
// Note that the `url` might be either 1) the last committed URL of `frame` or
// 2) the target of a ReadyToCommit navigation in `frame`.
//
// Note that this method ignores CSS content scripts.
bool DoContentScriptsMatch(const Extension& extension,
                           content::RenderFrameHost* frame,
                           const GURL& url) {
  const UserScriptList& list =
      ContentScriptsInfo::GetContentScripts(&extension);
  return std::any_of(list.begin(), list.end(),
                     [frame, &url](const std::unique_ptr<UserScript>& script) {
                       return DoesContentScriptMatch(*script, frame, url);
                     });
}

std::vector<const Extension*> GetExtensionsInjectingContentScripts(
    content::NavigationHandle* navigation) {
  content::RenderFrameHost* frame = navigation->GetRenderFrameHost();
  const GURL& url = navigation->GetURL();

  std::vector<const Extension*> extensions_injecting_content_scripts;
  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // This method shouldn't be called during shutdown.
  for (const auto& it : registry->enabled_extensions()) {
    const Extension& extension = *it;
    if (!DoContentScriptsMatch(extension, frame, url))
      continue;

    extensions_injecting_content_scripts.push_back(&extension);
  }

  return extensions_injecting_content_scripts;
}

}  // namespace

// static
bool ContentScriptTracker::DidFrameRunContentScriptFromExtension(
    content::RenderFrameHost* frame,
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!frame)
    return false;

  // Check the last committed URL - this is needed for URLs (like "about:blank")
  // that might not go through ReadyToCommit.  (Today "about:blank" should still
  // go through DidCommit, but there are tentative ideas/plans to avoid this and
  // therefore ContentScriptTracker looks at the last committed URL rather than
  // monitoring DidCommit IPCs.)
  const GURL& url = frame->GetLastCommittedURL();
  if (url.SchemeIs(url::kAboutScheme)) {
    const ExtensionRegistry* registry =
        ExtensionRegistry::Get(frame->GetBrowserContext());
    DCHECK(registry);  // This method shouldn't be called during shutdown.

    const Extension* extension =
        registry->enabled_extensions().GetByID(extension_id);
    if (extension && DoContentScriptsMatch(*extension, frame, url))
      return true;
  }

  // Check if we've been notified about the content script injection via
  // ReadyToCommitNavigation or WillExecuteCode methods.
  FrameToExtensionIdSet& frame_to_extension_id_set = GetFrameToExtensionIdSet();
  auto it = frame_to_extension_id_set.find(frame->GetGlobalFrameRoutingId());
  if (it == frame_to_extension_id_set.end())
    return false;

  const ExtensionIdSet& extension_id_set = it->second;
  return base::Contains(extension_id_set, extension_id);
}

// static
void ContentScriptTracker::ReadyToCommitNavigation(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::NavigationHandle* navigation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<const Extension*> extensions_injecting_content_scripts =
      GetExtensionsInjectingContentScripts(navigation);
  ExtensionIdSet& content_scripts_for_frame =
      GetOrCreateExtensionIdSet(navigation->GetRenderFrameHost());
  for (const Extension* extension : extensions_injecting_content_scripts)
    content_scripts_for_frame.insert(extension->id());

  URLLoaderFactoryManager::WillInjectContentScriptsWhenNavigationCommits(
      base::PassKey<ContentScriptTracker>(), navigation,
      extensions_injecting_content_scripts);
}

// static
void ContentScriptTracker::RenderFrameDeleted(
    base::PassKey<ExtensionWebContentsObserver> pass_key,
    content::RenderFrameHost* frame) {
  FrameToExtensionIdSet& frame_to_extension_id_set = GetFrameToExtensionIdSet();
  frame_to_extension_id_set.erase(frame->GetGlobalFrameRoutingId());
}

// static
void ContentScriptTracker::WillExecuteCode(
    base::PassKey<ScriptExecutor> pass_key,
    content::RenderFrameHost* frame,
    const mojom::HostID& host_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (host_id.type) {
    case mojom::HostID::HostType::kWebUi:
      // This class only tracks extensions.
      return;
    case mojom::HostID::HostType::kExtensions:
      break;
  }

  const ExtensionRegistry* registry =
      ExtensionRegistry::Get(frame->GetProcess()->GetBrowserContext());
  DCHECK(registry);  // WillExecuteCode shouldn't happen during shutdown.
  const Extension* extension =
      registry->enabled_extensions().GetByID(host_id.id);
  DCHECK(extension);  // Guaranteed by the caller - see the doc comment.

  HandleProgrammaticContentScriptInjection(PassKey(), frame, *extension);
}

// static
void ContentScriptTracker::WillExecuteCode(
    base::PassKey<RequestContentScript> pass_key,
    content::RenderFrameHost* frame,
    const Extension& extension) {
  HandleProgrammaticContentScriptInjection(PassKey(), frame, extension);
}

// static
bool ContentScriptTracker::DoContentScriptsMatchForTesting(
    const Extension& extension,
    content::RenderFrameHost* frame,
    const GURL& url) {
  return DoContentScriptsMatch(extension, frame, url);
}

}  // namespace extensions
