// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_service_consumer.h"

WebDataServiceConsumer::WebDataServiceConsumer() = default;
WebDataServiceConsumer::~WebDataServiceConsumer() = default;

base::WeakPtr<WebDataServiceConsumer>
WebDataServiceConsumer::GetWebDataServiceConsumerWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
