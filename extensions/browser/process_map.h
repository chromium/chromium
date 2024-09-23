// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MAP_H_
#define EXTENSIONS_BROWSER_PROCESS_MAP_H_

#include <stddef.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/site_instance.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;

// Contains information about which extensions are assigned to which processes.
//
// The relationship between extensions and processes is complex:
//
// - Extensions can be either "split" mode or "spanning" mode.
// - In spanning mode, extensions *generally* share a single process between all
//   incognito and normal windows. This was the original mode for extensions.
// - In split mode, extensions have separate processes in incognito windows.
// - There are also hosted apps, which are a kind of extensions, and those
//   usually have a process model similar to normal web sites: multiple
//   processes per-profile.
// - An extension can also opt into Cross Origin Isolation in which case it can
//   have multiple processes per profile since cross-origin-isolated and
//   non-cross-origin-isolated contexts don't share a process.
//
// Under the current model, a single extension can correspond to multiple
// processes (see explanation below), but a single process cannot be shared by
// multiple extensions.
//
// Note that because of content scripts, frames, and other edge cases in
// Chrome's process isolation, extension code can still end up running outside
// an assigned process.
//
// But we only allow high-privilege operations to be performed by an extension
// when it is running in an assigned process.
//
// ===========================================================================
// WARNINGS - PLEASE UNDERSTAND THESE BEFORE CALLING OR MODIFYING THIS CLASS
// ===========================================================================
//
// 1. This class contains the processes for hosted apps as well as extensions
//    and packaged apps. Just because a process is present here *does not* mean
//    it is an "extension process" (e.g., for UI purposes). It may contain only
//    hosted apps. See crbug.com/102533.
//
// 2. An extension can show up in multiple processes. That is why there is no
//    GetExtensionProcess() method here. There are multiple such cases:
//      a) The extension is actually a hosted app.
//      b) There is an incognito window open and the extension is "split mode".
//      c) The extension is cross origin isolated but has
//         non-cross-origin-isolated contexts.
//    It is *not safe* to assume that there is one process per extension.
//
// 3. The process ids contained in this class are *not limited* to the Profile
//    you got this map from. They can also be associated with that profile's
//    incognito/normal twin. If you care about this, use
//    RenderProcessHost::FromID() and check the profile of the resulting object.
//
// TODO(aa): The above warnings suggest this class could use improvement :).
//
// TODO(kalman): This class is not threadsafe, but is used on both the UI and IO
//               threads. Somebody should fix that, either make it threadsafe or
//               enforce single thread. Investigation required.
class ProcessMap : public KeyedService {
 public:
  explicit ProcessMap(content::BrowserContext* browser_context);

  ProcessMap(const ProcessMap&) = delete;
  ProcessMap& operator=(const ProcessMap&) = delete;

  ~ProcessMap() override;

  void Shutdown() override;

  // Returns the instance for |browser_context|. An instance is shared between
  // an incognito and a regular context.
  static ProcessMap* Get(content::BrowserContext* browser_context);

  size_t size() const { return items_.size(); }

  bool Insert(const ExtensionId& extension_id, int process_id);

  int Remove(int process_id);

  bool Contains(const ExtensionId& extension_id, int process_id) const;
  bool Contains(int process_id) const;

  // Returns a pointer to an enabled extension running in `process_id` or
  // nullptr.
  const Extension* GetEnabledExtensionByProcessID(int process_id) const;

  std::optional<ExtensionId> GetExtensionIdForProcess(int process_id) const;

  // Returns true if the given `process_id` is considered a privileged context
  // for the given `extension`. That is, if it would *probably* correspond to a
  // mojom::ContextType::kPrivilegedExtension.
  // NOTE: There are circumstances in which a context from a privileged
  // extension *process* may not correspond to a privileged extension *context*
  // (mojom::ContextType::kPrivilegedExtension).
  // These include, for instance, sandboxed extension frames or offscreen
  // documents, which run in the same process, but are not considered
  // privileged contexts.
  // However, these are not necessarily security bugs. There is no security
  // boundary between an extension's offscreen document and other frames, and
  // extension sandboxed frames behave slightly differently than sandboxed pages
  // on the web.
  bool IsPrivilegedExtensionProcess(const Extension& extension, int process_id);

  // Returns true if the given `context_type` - associated with the given
  // `extension`, if provided - is valid for the given `process`.
  //
  // Use this method to validate whether a context type claimed by the renderer
  // is possible.
  //
  // Important notes:
  // - This will return false for any invalid combinations. For instance, it is
  //   never possible to have a web page context associated with an extension.
  // - This relies on certain architectural guarantees. For instance, web pages
  //   should never, ever share a process with an extension or with webui.
  // - Multiple context types (with some difference in privilege levels) may be
  //   valid for a given process and extension pairing. For instance, a
  //   privileged extension process could host any of privileged extension
  //   contexts, offscreen document contexts, and content script contexts. Thus,
  //   a compromised renderer could, in theory, claim a more privileged context
  //   (such as claiming to be a privileged extension context from an offscreen
  //   document context). This *is not* a security bug; if the renderer is
  //   compromised and could host privileged extension contexts, it could simply
  //   create (or hijack) one.
  // - This only looks at process-level guarantees. Thus, for contexts like
  //   untrusted webui (chrome-untrusted:// pages), the caller is responsible
  //   for doing additional verification (such as checking the origin).
  //
  // This method is preferable to GetMostLikelyContextType() as it allows the
  // renderer to supply a context type to differentiate between possible
  // contexts in the non-compromised-renderer case, whereas
  // GetMostLikelyContextType() cannot (and has to just "pick" a possible
  // context type).
  bool CanProcessHostContextType(const Extension* extension,
                                 const content::RenderProcessHost& process,
                                 mojom::ContextType context_type);

  // Gets the most likely context type for the process with ID |process_id|
  // which hosts Extension |extension|, if any (may be nullptr). Context types
  // are renderer (JavaScript) concepts but the browser can do a decent job in
  // guessing what the process hosts.
  //
  // For Context types with no |extension| e.g. untrusted WebUIs, we use |url|
  // which should correspond to the URL where the API is running.|url| could be
  // the frame's URL, the Content Script's URL, or the URL where a Content
  // Script is running. So |url| should only be used when there is no
  // |extension|. |url| may be also be nullptr when running in Service Workers.
  // Currently, the |url| provided by event_router.cc is passed from the
  // renderer process and therefore can't be fully trusted.
  // TODO(ortuno): Change call sites to only pass in a URL when |extension| is
  // nullptr and only use a URL retrieved from the browser process.
  //
  // |extension| is the funky part - unfortunately we need to trust the
  // caller of this method to be correct that indeed the context does feature
  // an extension. This matters for iframes, where an extension could be
  // hosted in another extension's process (privilege level needs to be
  // downgraded) or in a web page's process (privilege level needs to be
  // upgraded).
  //
  // The latter of these is slightly problematic from a security perspective;
  // if a web page renderer gets owned it could try to pretend it's an
  // extension and get access to some unprivileged APIs. Luckly, when OOP
  // iframes lauch, it won't be an issue.
  //
  // Anyhow, the expected behaviour is:
  //   - For hosted app processes, this will be `kPrivilegedWebPage`.
  //   - For processes of platform apps running on lock screen, this will be
  //     `kLockscreenExtension`.
  //   - For other extension processes, this will be `kPrivilegedExtension`.
  //   - For WebUI processes, this will be `kWebUi`.
  //   - For chrome-untrusted:// URLs, this will be a `kUntrustedWebUi`.
  //   - For any other extension we have the choice of `kUnprivilegedExtension`
  //     or `kContentScript`. Since content scripts are more common, guess that.
  //     We *could* in theory track which web processes have extension frames
  //     in them, and those would be `kUnprivilegedExtension`, but we don't at
  //     the moment, and once OOP iframes exist then there won't even be such a
  //     thing as a `kUnprivilegedExtension` context.
  //   - For anything else, `kWebPage`.
  virtual mojom::ContextType GetMostLikelyContextType(
      const Extension* extension,
      int process_id,
      const GURL* url) const;

  void set_is_lock_screen_context(bool is_lock_screen_context) {
    is_lock_screen_context_ = is_lock_screen_context;
  }

 private:
  using ProcessId = int;

  base::flat_map<ProcessId, ExtensionId> items_;

  // Whether the process map belongs to the browser context used on Chrome OS
  // lock screen.
  bool is_lock_screen_context_ = false;

  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MAP_H_
