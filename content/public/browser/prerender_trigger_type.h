// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_TRIGGER_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_TRIGGER_TYPE_H_

namespace content {

enum class PrerenderTriggerType {
  // https://wicg.github.io/nav-speculation/prerendering.html#speculation-rules
  kSpeculationRule,
  // Trigger used by content embedders.
  kEmbedder,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_TRIGGER_TYPE_H_
