// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_COMMON_URL_REQUEST_REWRITE_RULES_H_
#define FUCHSIA_ENGINE_COMMON_URL_REQUEST_REWRITE_RULES_H_

#include "base/memory/ref_counted.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"

namespace url_rewrite {

using UrlRequestRewriteRules =
    base::RefCountedData<mojom::UrlRequestRewriteRulesPtr>;

}

#endif  // FUCHSIA_ENGINE_COMMON_URL_REQUEST_REWRITE_RULES_H_
