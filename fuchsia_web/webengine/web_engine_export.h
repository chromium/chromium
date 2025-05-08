// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_EXPORT_H_
#define FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_EXPORT_H_

#if defined(COMPONENT_BUILD)

#define WEB_ENGINE_EXPORT __attribute__((visibility("default")))

#else  // defined(COMPONENT_BUILD)
#define WEB_ENGINE_EXPORT
#endif

#endif  // FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_EXPORT_H_
