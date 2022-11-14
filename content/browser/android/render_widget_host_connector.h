// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_RENDER_WIDGET_HOST_CONNECTOR_H_
#define CONTENT_BROWSER_ANDROID_RENDER_WIDGET_HOST_CONNECTOR_H_

#include "base/gtest_prod_util.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// A base class used to connect an object of the content layer lifecycle
// to |RenderWidgetHostViewAndroid|. The inherting class needs to
// override |UpdateRenderProcessConnection| to set itself to the RWHVA
// brought up foreground, and null out its reference in the RWHVA going
// away so it won't access the object any more.
// This class owns itself and gets deleted when the WebContents is deleted.
// Can also delete this object early (before the WebContents is destroyed)
// by calling Destroy directly.
class RenderWidgetHostConnector {
 public:
  explicit RenderWidgetHostConnector(WebContents* web_contents);

  RenderWidgetHostConnector(const RenderWidgetHostConnector&) = delete;
  RenderWidgetHostConnector& operator=(const RenderWidgetHostConnector&) =
      delete;

  virtual ~RenderWidgetHostConnector();

  // Establish initial connection to rwhva if it is present.
  void Initialize();

  // Method to set itself to the |new_rwhva|, and null out the reference
  // in |old_rwvha|. Example:
  //
  // if (old_rwhva)
  //   old_rwhva->set_object(nullptr);
  // if (new_rwhva)
  //   new_rwhva->set_object(this);
  virtual void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) = 0;

  RenderWidgetHostViewAndroid* GetRWHVAForTesting() const;

 protected:
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostConnectorTest, DestroyEarly);

  WebContents* web_contents() const;

  // Deletes this now. Note this is usually not required as this is
  // deleted when the corresponding WebContents is destroyed.
  void DestroyEarly();

 private:
  class Observer;
  std::unique_ptr<Observer> render_widget_observer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_RENDER_WIDGET_HOST_CONNECTOR_H_
