// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/interceptable_pref_filter.h"

#include <utility>

#include "base/bind.h"

InterceptablePrefFilter::InterceptablePrefFilter() {}
InterceptablePrefFilter::~InterceptablePrefFilter() {}

void InterceptablePrefFilter::FilterOnLoad(
    PostFilterOnLoadCallback post_filter_on_load_callback,
    std::unique_ptr<base::DictionaryValue> pref_store_contents) {
  if (filter_on_load_interceptor_.is_null()) {
    FinalizeFilterOnLoad(std::move(post_filter_on_load_callback),
                         std::move(pref_store_contents), false);
  } else {
    // Note, in practice (in the implementation as it was in May 2014) it would
    // be okay to pass an unretained |this| pointer below, but in order to avoid
    // having to augment the API everywhere to explicitly enforce the ownership
    // model as it happens to currently be: make the relationship simpler by
    // weakly binding the FinalizeFilterOnLoadCallback below to |this|.
    FinalizeFilterOnLoadCallback finalize_filter_on_load(
        base::BindOnce(&InterceptablePrefFilter::FinalizeFilterOnLoad,
                       AsWeakPtr(), std::move(post_filter_on_load_callback)));
    std::move(filter_on_load_interceptor_)
        .Run(std::move(finalize_filter_on_load),
             std::move(pref_store_contents));
  }
}

void InterceptablePrefFilter::InterceptNextFilterOnLoad(
    FilterOnLoadInterceptor filter_on_load_interceptor) {
  DCHECK(filter_on_load_interceptor_.is_null());
  filter_on_load_interceptor_ = std::move(filter_on_load_interceptor);
}

void InterceptablePrefFilter::OnStoreDeletionFromDisk() {}
