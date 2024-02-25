// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTEXT_MENU_PARAMS_H_
#define CONTENT_PUBLIC_BROWSER_CONTEXT_MENU_PARAMS_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// FIXME(beng): This would be more useful in the future and more efficient
//              if the parameters here weren't so literally mapped to what
//              they contain for the ContextMenu task. It might be better
//              to make the string fields more generic so that this object
//              could be used for more contextual actions.
//
// SECURITY NOTE: This struct should be populated by the browser process,
// after validating the IPC payload from blink::UntrustworthyContextMenuParams.
// Note that the fields declared in ContextMenuParams can be populated based on
// the trustworthy, browser-side data (i.e. don't need to be sent over IPC and
// therefore don't need to be covered by blink::UntrustworthyContextMenuParams).
struct CONTENT_EXPORT ContextMenuParams
    : public blink::UntrustworthyContextMenuParams {
  ContextMenuParams();
  ContextMenuParams(const ContextMenuParams& other);
  ~ContextMenuParams();

  // This is the URL of the top level page that the context menu was invoked
  // on.
  GURL page_url;

  // This is the URL of the frame that the context menu was invoked on. This may
  // or may not be equal to `page_url`.
  GURL frame_url;

  // The origin of the frame that the context menu was invoked on. This is *not*
  // the same as Origin::Create(frame_url) for the reasons given in
  // //docs/security/origin-vs-url.md.
  url::Origin frame_origin;

  // Whether the context menu was invoked on a subframe.
  bool is_subframe = false;

  // Extra properties for the context menu.
  std::map<std::string, std::string> properties;

 private:
  // RenderFrameHostImpl is responsible for validating and sanitizing
  // blink::UntrustworthyContextMenuParams into ContextMenuParams and therefore
  // is a friend.
  friend class RenderFrameHostImpl;
  explicit ContextMenuParams(
      const blink::UntrustworthyContextMenuParams& other);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTEXT_MENU_PARAMS_H_
