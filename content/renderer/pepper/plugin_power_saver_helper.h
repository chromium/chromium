// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "url/origin.h"

namespace gfx {
class Size;
}

namespace content {

class CONTENT_EXPORT PluginPowerSaverHelper : public RenderFrameObserver {
 public:
  explicit PluginPowerSaverHelper(RenderFrame* render_frame);
  ~PluginPowerSaverHelper() override;

 private:
  friend class RenderFrameImpl;

  struct PeripheralPlugin {
    PeripheralPlugin(const url::Origin& content_origin,
                     base::OnceClosure unthrottle_callback);
    ~PeripheralPlugin();

    PeripheralPlugin(PeripheralPlugin&&);
    PeripheralPlugin& operator=(PeripheralPlugin&&);

    url::Origin content_origin;
    base::OnceClosure unthrottle_callback;
  };

  // See RenderFrame for documentation.
  void RegisterPeripheralPlugin(const url::Origin& content_origin,
                                base::OnceClosure unthrottle_callback);
  RenderFrame::PeripheralContentStatus GetPeripheralContentStatus(
      const url::Origin& main_frame_origin,
      const url::Origin& content_origin,
      const gfx::Size& unobscured_size,
      RenderFrame::RecordPeripheralDecision record_decision) const;
  void WhitelistContentOrigin(const url::Origin& content_origin);

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  void OnUpdatePluginContentOriginWhitelist(
      const std::set<url::Origin>& origin_whitelist);

  // Local copy of the whitelist for the entire tab.
  std::set<url::Origin> origin_whitelist_;

  // Set of peripheral plugins eligible to be unthrottled ex post facto.
  std::vector<PeripheralPlugin> peripheral_plugins_;

  DISALLOW_COPY_AND_ASSIGN(PluginPowerSaverHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_
