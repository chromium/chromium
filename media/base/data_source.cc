// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_source.h"


namespace media {

DataSourceInfo::~DataSourceInfo() = default;

DataSource::DataSource() = default;

DataSource::~DataSource() = default;

bool DataSource::AssumeFullyBuffered() const {
  return true;
}

int64_t DataSource::GetMemoryUsage() {
  int64_t temp;
  return GetSize(&temp) ? temp : 0;
}

void DataSource::SetPreload(media::DataSource::Preload preload) {}

GURL DataSource::GetUrlAfterRedirects() const {
  return GURL();
}

void DataSource::OnBufferingHaveEnough(bool must_cancel_netops) {}

void DataSource::OnMediaPlaybackRateChanged(double playback_rate) {}

void DataSource::OnMediaIsPlaying() {}

CrossOriginDataSource* DataSource::GetAsCrossOriginDataSource() {
  return nullptr;
}

}  // namespace media
