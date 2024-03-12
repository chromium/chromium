// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_promo_event_exporter.h"

DefaultBrowserEventExporter::DefaultBrowserEventExporter() {}

DefaultBrowserEventExporter::~DefaultBrowserEventExporter() = default;

void DefaultBrowserEventExporter::ExportEvents(ExportEventsCallback callback) {
  std::vector<EventData> events_to_migrate;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(events_to_migrate)));
}
