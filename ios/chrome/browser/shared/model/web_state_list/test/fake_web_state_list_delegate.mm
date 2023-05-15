// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeWebStateListDelegate::FakeWebStateListDelegate() = default;

FakeWebStateListDelegate::~FakeWebStateListDelegate() = default;

void FakeWebStateListDelegate::WillAddWebState(web::WebState* web_state) {}

void FakeWebStateListDelegate::WebStateDetached(web::WebState* web_state) {}
