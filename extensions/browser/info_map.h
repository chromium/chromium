// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INFO_MAP_H_
#define EXTENSIONS_BROWSER_INFO_MAP_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_set.h"

namespace extensions {
class ContentVerifier;
class Extension;
enum class UnloadedExtensionReason;

// Contains extension data that needs to be accessed on the IO thread. It can
// be created on any thread, but all other methods and destructor must be called
// on the IO thread.
class InfoMap : public base::RefCountedThreadSafe<
                    InfoMap,
                    content::BrowserThread::DeleteOnIOThread> {
 public:
  InfoMap();

  const ExtensionSet& extensions() const;

  // Callback for when new extensions are loaded.
  // TODO(karandeepb): Some of these arguments are unused. Remove them.
  void AddExtension(const Extension* extension,
                    base::Time install_time,
                    bool incognito_enabled,
                    bool notifications_disabled);

  // Callback for when an extension is unloaded.
  void RemoveExtension(const std::string& extension_id);

  void SetContentVerifier(ContentVerifier* verifier);
  ContentVerifier* content_verifier() { return content_verifier_.get(); }

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<InfoMap>;

  ~InfoMap();

  ExtensionSet extensions_;

  scoped_refptr<ContentVerifier> content_verifier_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INFO_MAP_H_
