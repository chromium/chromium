// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_METRICKIT_METRIC_PAYLOAD_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_METRICKIT_METRIC_PAYLOAD_H_

#import <Foundation/Foundation.h>

// Returns a mock MXMetricPayload given its description as an NSDictionary.
// Keys in the dictionary are the same in the JSON export of an MXMetricPayload.
// Values are simplified:
// - An NSMeasurement and MXAverage are just an NSNumber with the value
// - An MXHistogram is given by a dictionary where keys is the bucket and value
//   is the bucket count. E.g.
//   @"histogrammedTimeToFirstDrawKey" : @{@5 : @2, @15 : @4}
//   will report 2 startup in bucket [0,10] and 4 in bucket [10,20].
// Only the values currently reported in Chrome are mocked.
id MockMetricPayload(NSDictionary* dictionary);

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_METRICKIT_METRIC_PAYLOAD_H_
