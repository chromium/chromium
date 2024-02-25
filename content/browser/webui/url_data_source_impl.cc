// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_source_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

namespace content {

URLDataSourceImpl::URLDataSourceImpl(const std::string& source_name,
                                     std::unique_ptr<URLDataSource> source)
    : source_name_(source_name), source_(std::move(source)) {}

URLDataSourceImpl::~URLDataSourceImpl() {
}

bool URLDataSourceImpl::IsWebUIDataSourceImpl() const {
  return false;
}

}  // namespace content
