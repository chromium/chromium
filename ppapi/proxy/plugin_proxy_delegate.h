// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_
#define PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_

#include <string>

namespace IPC {
class Sender;
}

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT PluginProxyDelegate {
 public:
  virtual ~PluginProxyDelegate() {}

  // Returns the channel for sending to the browser.
  // Note: The returned sender must be thread-safe. It might be used while the
  // proxy lock is not acquired. Please see the implementation of
  // PluginGlobals::BrowserSender.
  virtual IPC::Sender* GetBrowserSender() = 0;

  // Returns the language code of the current UI language.
  virtual std::string GetUILanguage() = 0;

  // Sets the active url which is reported by breakpad.
  virtual void SetActiveURL(const std::string& url) = 0;

  // Validates the font description, and uses it to create a
  // BrowserFontResource_Trusted resource.
  virtual PP_Resource CreateBrowserFont(
      Connection connection,
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description& desc,
      const Preferences& prefs) = 0;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_PROXY_DELEGATE_H_
