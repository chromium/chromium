// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_FACTORY_H_

#include <stdint.h>

#include "content/browser/renderer_host/browsing_context_state.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHostImpl;
class PageDelegate;

// A factory for creating Page. There is a global factory function that can be
// installed for the purposes of testing to provide a specialized Page class.
class PageFactory {
 public:
  // Creates a Page using the currently registered factory, or the default one
  // if no factory is registered. Ownership of the returned pointer will be
  // passed to the caller.
  static std::unique_ptr<PageImpl> Create(RenderFrameHostImpl& rfh,
                                          PageDelegate& delegate);

  PageFactory(const PageFactory&) = delete;
  PageFactory& operator=(const PageFactory&) = delete;

  // Returns true if there is currently a globally-registered factory.
  static bool has_factory() { return !!factory_; }

 protected:
  PageFactory() = default;
  virtual ~PageFactory() = default;

  // You can derive from this class and specify an implementation for this
  // function to create a different kind of Page for testing.
  virtual std::unique_ptr<PageImpl> CreatePage(RenderFrameHostImpl& rfh,
                                               PageDelegate& delegate) = 0;

  // Registers your factory to be called when new Page are created.
  // We have only one global factory, so there must be no factory registered
  // before the call. This class does NOT take ownership of the pointer.
  CONTENT_EXPORT static void RegisterFactory(PageFactory* factory);

  // Unregister the previously registered factory. With no factory registered,
  // the default Page will be created.
  CONTENT_EXPORT static void UnregisterFactory();

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default Page.
  CONTENT_EXPORT static PageFactory* factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_FACTORY_H_
