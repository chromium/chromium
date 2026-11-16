// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_handler.h"

#import <algorithm>
#import <ostream>
#import <vector>

#import "base/check.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_view_provider.h"

@implementation GeminiCustomUIHandler {
  // Added providers. Each renders a distinct type of block.
  std::vector<__weak id<GeminiCustomUIViewProvider>> _providers;
}

#pragma mark - Public

- (void)addViewProvider:(id<GeminiCustomUIViewProvider>)provider {
  CHECK(provider);
  if (std::ranges::contains(_providers, provider)) {
    return;
  }
  std::erase_if(_providers, [](id entry) { return entry == nil; });
  _providers.push_back(provider);
}

- (void)removeViewProvider:(id<GeminiCustomUIViewProvider>)provider {
  CHECK(provider);
  std::erase(_providers, provider);
}

#pragma mark - GeminiCustomUIDelegate

- (BOOL)providesCustomContentViewForBlockNode:(id<GeminiBlockNode>)blockNode {
  return [self providerForBlockNode:blockNode] != nil;
}

- (BOOL)shouldHideBlockNode:(id<GeminiBlockNode>)blockNode {
  // Hiding drops a block before any view is requested; see
  // `GeminiCustomUIViewProvider hidesBlock:`.
  for (id<GeminiCustomUIViewProvider> provider : _providers) {
    if ([provider hidesBlock:blockNode]) {
      return YES;
    }
  }
  return NO;
}

- (UIView*)customContentViewForBlockNode:(id<GeminiBlockNode>)blockNode
                                 context:(id<GeminiBlockContext>)context
                             reusingView:(UIView*)view {
  id<GeminiCustomUIViewProvider> provider =
      [self providerForBlockNode:blockNode];
  // The provider was removed or deallocated after claiming the block. The SDK
  // requires a view here; returning nil would crash its Swift caller.
  if (!provider) {
    return view ?: [[UIView alloc] init];
  }
  return [provider viewForBlock:blockNode context:context reusingView:view];
}

#pragma mark - Private

// Returns the provider that renders `blockNode`, or nil if none does.
- (id<GeminiCustomUIViewProvider>)providerForBlockNode:
    (id<GeminiBlockNode>)blockNode {
  id<GeminiCustomUIViewProvider> match = nil;
  for (id<GeminiCustomUIViewProvider> provider : _providers) {
    if (![provider providesViewForBlock:blockNode]) {
      continue;
    }
    CHECK(!match) << "Several providers render the same block.";
    match = provider;
  }
  return match;
}

@end
