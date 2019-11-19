// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/interstitials/ios_chrome_metrics_helper.h"

#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/rappor/rappor_service_impl.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeMetricsHelper::IOSChromeMetricsHelper(
    web::WebState* web_state,
    const GURL& request_url,
    const security_interstitials::MetricsHelper::ReportDetails report_details)
    : security_interstitials::MetricsHelper(
          request_url,
          report_details,
          ios::HistoryServiceFactory::GetForBrowserState(
              ios::ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState()),
              ServiceAccessType::EXPLICIT_ACCESS)) {}

IOSChromeMetricsHelper::~IOSChromeMetricsHelper() {}

void IOSChromeMetricsHelper::RecordExtraUserDecisionMetrics(
    security_interstitials::MetricsHelper::Decision decision) {}

void IOSChromeMetricsHelper::RecordExtraUserInteractionMetrics(
    security_interstitials::MetricsHelper::Interaction interaction) {}

void IOSChromeMetricsHelper::RecordExtraShutdownMetrics() {}
