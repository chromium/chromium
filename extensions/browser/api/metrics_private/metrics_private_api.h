// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_

#include <stddef.h>

#include <string>

#include "base/metrics/histogram.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class MetricsPrivateGetIsCrashReportingEnabledFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.getIsCrashReportingEnabled",
                             METRICSPRIVATE_GETISCRASHRECORDINGENABLED)

 protected:
  ~MetricsPrivateGetIsCrashReportingEnabledFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateGetFieldTrialFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.getFieldTrial",
                             METRICSPRIVATE_GETFIELDTRIAL)

 protected:
  ~MetricsPrivateGetFieldTrialFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateGetVariationParamsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.getVariationParams",
                             METRICSPRIVATE_GETVARIATIONPARAMS)

 protected:
  ~MetricsPrivateGetVariationParamsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordUserActionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordUserAction",
                             METRICSPRIVATE_RECORDUSERACTION)

 protected:
  ~MetricsPrivateRecordUserActionFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsHistogramHelperFunction : public ExtensionFunction {
 protected:
  ~MetricsHistogramHelperFunction() override {}
  void RecordValue(const std::string& name,
                   base::HistogramType type,
                   int min,
                   int max,
                   size_t buckets,
                   int sample);
};

class MetricsPrivateRecordValueFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordValue",
                             METRICSPRIVATE_RECORDVALUE)

 protected:
  ~MetricsPrivateRecordValueFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordBooleanFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordBoolean",
                             METRICSPRIVATE_RECORDBOOLEAN)

 protected:
  ~MetricsPrivateRecordBooleanFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordEnumerationValueFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordEnumerationValue",
                             METRICSPRIVATE_RECORDENUMERATIONVALUE)

 protected:
  ~MetricsPrivateRecordEnumerationValueFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordSparseValueWithHashMetricNameFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "metricsPrivate.recordSparseValueWithHashMetricName",
      METRICSPRIVATE_RECORDSPARSEVALUEWITHHASHMETRICNAME)

 protected:
  ~MetricsPrivateRecordSparseValueWithHashMetricNameFunction() override =
      default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordSparseValueWithPersistentHashFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "metricsPrivate.recordSparseValueWithPersistentHash",
      METRICSPRIVATE_RECORDSPARSEVALUEWITHPERSISTENTHASH)

 protected:
  ~MetricsPrivateRecordSparseValueWithPersistentHashFunction() override =
      default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordSparseValueFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordSparseValue",
                             METRICSPRIVATE_RECORDSPARSEVALUE)

 protected:
  ~MetricsPrivateRecordSparseValueFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordPercentageFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordPercentage",
                             METRICSPRIVATE_RECORDPERCENTAGE)

 protected:
  ~MetricsPrivateRecordPercentageFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordCount",
                             METRICSPRIVATE_RECORDCOUNT)

 protected:
  ~MetricsPrivateRecordCountFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordSmallCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordSmallCount",
                             METRICSPRIVATE_RECORDSMALLCOUNT)

 protected:
  ~MetricsPrivateRecordSmallCountFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordMediumCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumCount",
                             METRICSPRIVATE_RECORDMEDIUMCOUNT)

 protected:
  ~MetricsPrivateRecordMediumCountFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordTime",
                             METRICSPRIVATE_RECORDTIME)

 protected:
  ~MetricsPrivateRecordTimeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordMediumTimeFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumTime",
                             METRICSPRIVATE_RECORDMEDIUMTIME)

 protected:
  ~MetricsPrivateRecordMediumTimeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateRecordLongTimeFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordLongTime",
                             METRICSPRIVATE_RECORDLONGTIME)

 protected:
  ~MetricsPrivateRecordLongTimeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MetricsPrivateGetHistogramFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.getHistogram",
                             METRICSPRIVATE_GETHISTOGRAM)

 private:
  ~MetricsPrivateGetHistogramFunction() override;
  ResponseAction Run() override;

  // Sends an asynchronous response containing data for the histogram named
  // |name|. Passed to content::FetchHistogramsAsynchronously() to be run after
  // new data from other processes has been collected.
  void RespondOnHistogramsFetched(const std::string& name);

  // Creates a response with current data for the histogram named |name|.
  ResponseValue GetHistogram(const std::string& name);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_
