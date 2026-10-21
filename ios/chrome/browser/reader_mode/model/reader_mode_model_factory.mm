// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_model_factory.h"

#import <utility>

#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"

ReaderModeModelFactory::ReaderModeModelFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key), "ReaderModeModel") {}

std::unique_ptr<KeyedService> ReaderModeModelFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ReaderModeModel>(profile);
}
