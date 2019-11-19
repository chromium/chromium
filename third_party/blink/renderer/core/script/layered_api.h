// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_LAYERED_API_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_LAYERED_API_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Modulator;

// Implements Layered API.
// Spec: https://github.com/drufball/layered-apis/blob/master/spec.md
// Implementation Design Doc:
// https://docs.google.com/document/d/1V-WaCZQbBcQJRSYSYBb8Y6p0DOdDpiNDSmD41ui_73s/edit

// Some methods takes Modulator as an argument, to support origin trials
// (which depends on ExecutionContext).
// TODO(hiroshige): Using Modulator here means Layered API fetching and
// import maps fallback depends on module map settings object. Currently
// this is consistent throughout Blink implementation, but this might cause
// problems when we support built-in modules around workers
// (https://crbug.com/927477), and we might switch to fetch client settings
// object in the future (https://crbug.com/928435).
namespace layered_api {

// Returns the path part (`x`) for std:x or import:@std/x URLs.
// For other URLs, returns a null String.
//
// Currently accepts both "std:x" and "import:@std/x", but
// because the spec discussion about notation is ongoing:
// https://github.com/tc39/proposal-javascript-standard-library/issues/12
// TODO(hiroshige): Update the implementation once the discussion converges.
CORE_EXPORT String GetBuiltinPath(const KURL&);

// https://github.com/drufball/layered-apis/blob/master/spec.md#user-content-layered-api-fetching-url
//
// Currently fallback syntax is disabled and only "std:x" (not "std:x|y") is
// accepted. https://crbug.com/864748
CORE_EXPORT KURL ResolveFetchingURL(const Modulator&, const KURL&);

// Returns std-internal://x/index.mjs if the URL is Layered API, or null URL
// otherwise (not specced).
CORE_EXPORT KURL GetInternalURL(const KURL&);

// Gets source text for std-internal://x/index.mjs.
CORE_EXPORT String GetSourceText(const Modulator&, const KURL&);

}  // namespace layered_api

}  // namespace blink

#endif
