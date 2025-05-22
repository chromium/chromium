// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_

namespace optimization_guide {
namespace proto {
class PageContext;
}  // namespace proto
}  // namespace optimization_guide

@class GLICConsentMediator;

// Delegate for the GLICMediator.
@protocol GLICMediatorDelegate

// Dismisses the GLIC consent UI.
- (void)dismissGLICConsentUI;

// Opens GLIC Overlay.
- (void)openGLICOverlayForPage:
    (std::unique_ptr<optimization_guide::proto::PageContext>)pageContext;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_
