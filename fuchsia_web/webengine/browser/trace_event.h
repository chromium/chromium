// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_TRACE_EVENT_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_TRACE_EVENT_H_

// The category to use for WebEngine FIDL events.
// The event names should use the convention `namespace`/`interface`.`method`,
// e.g. "fuchsia.web/Frame.CreateView".
inline constexpr char kWebEngineFidlCategory[] = "webengine.fidl";

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_TRACE_EVENT_H_
