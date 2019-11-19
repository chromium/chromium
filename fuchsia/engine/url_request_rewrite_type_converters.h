// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
#define FUCHSIA_ENGINE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_

#include <fuchsia/web/cpp/fidl.h>

#include "fuchsia/base/string_util.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<mojom::UrlRequestRewriteAddHeadersPtr,
                     fuchsia::web::UrlRequestRewriteAddHeaders> {
  static mojom::UrlRequestRewriteAddHeadersPtr Convert(
      const fuchsia::web::UrlRequestRewriteAddHeaders& input);
};

template <>
struct TypeConverter<mojom::UrlRequestRewriteRemoveHeaderPtr,
                     fuchsia::web::UrlRequestRewriteRemoveHeader> {
  static mojom::UrlRequestRewriteRemoveHeaderPtr Convert(
      const fuchsia::web::UrlRequestRewriteRemoveHeader& input);
};

template <>
struct TypeConverter<mojom::UrlRequestRewriteSubstituteQueryPatternPtr,
                     fuchsia::web::UrlRequestRewriteSubstituteQueryPattern> {
  static mojom::UrlRequestRewriteSubstituteQueryPatternPtr Convert(
      const fuchsia::web::UrlRequestRewriteSubstituteQueryPattern& input);
};

template <>
struct TypeConverter<mojom::UrlRequestRewriteReplaceUrlPtr,
                     fuchsia::web::UrlRequestRewriteReplaceUrl> {
  static mojom::UrlRequestRewriteReplaceUrlPtr Convert(
      const fuchsia::web::UrlRequestRewriteReplaceUrl& input);
};

template <>
struct TypeConverter<mojom::UrlRequestRewritePtr,
                     fuchsia::web::UrlRequestRewrite> {
  static mojom::UrlRequestRewritePtr Convert(
      const fuchsia::web::UrlRequestRewrite& input);
};

template <>
struct TypeConverter<mojom::UrlRequestRewriteRulePtr,
                     fuchsia::web::UrlRequestRewriteRule> {
  static mojom::UrlRequestRewriteRulePtr Convert(
      const fuchsia::web::UrlRequestRewriteRule& input);
};

}  // namespace mojo

#endif  // FUCHSIA_ENGINE_URL_REQUEST_REWRITE_TYPE_CONVERTERS_H_
