// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_histograms_extension.h"

#include <limits.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_thread.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/function_template.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"

namespace content {

struct MetricType {
  std::string metric_name;
  std::string type;
  int min;
  int max;
  int buckets;
};

}  // namespace content

namespace gin {

template <>
struct Converter<content::MetricType> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     content::MetricType* out) {
    if (!val->IsObject()) {
      return false;
    }
    gin::Dictionary dict(isolate, val.As<v8::Object>());
    return dict.Get("metricName", &out->metric_name) &&
           dict.Get("type", &out->type) && dict.Get("min", &out->min) &&
           dict.Get("max", &out->max) && dict.Get("buckets", &out->buckets);
  }
};

}  // namespace gin

namespace content {

namespace {

const size_t kMaxBuckets = 10000;

// Exempts histograms recorded via chrome.histograms from Webium name mapping
// so top-chrome WebUI metrics are not dropped.
constexpr int32_t kWebUIHistogramFlags =
    base::HistogramBase::kUmaTargetedHistogramFlag |
    base::HistogramBase::kExemptFromNameMappingFlag;

// Implementation moved from
// extensions/browser/api/metrics_private/metrics_private_api.cc
void RecordValue(const std::string& name,
                 base::HistogramType type,
                 int min,
                 int max,
                 size_t buckets,
                 int sample) {
  // Make sure toxic values don't get to internal code.
  // Fix for maximums
  min = std::min(min, INT_MAX - 3);
  max = std::min(max, INT_MAX - 3);
  buckets = std::min(buckets, kMaxBuckets);
  // Fix for minimums.
  min = std::max(min, 1);
  max = std::max(max, min + 1);
  buckets = std::max(buckets, static_cast<size_t>(3));
  // Trim buckets down to a maximum of the given range + over/underflow buckets
  if (buckets > static_cast<size_t>(max - min + 2)) {
    buckets = max - min + 2;
  }

  base::HistogramBase* counter;
  if (type == base::LINEAR_HISTOGRAM) {
    counter = base::LinearHistogram::FactoryGet(name, min, max, buckets,
                                                kWebUIHistogramFlags);
  } else {
    counter = base::Histogram::FactoryGet(name, min, max, buckets,
                                          kWebUIHistogramFlags);
  }

  // The histogram can be NULL if it is constructed with bad arguments.  Ignore
  // that data for this API.  An error message will be logged.
  if (counter) {
    counter->Add(sample);
  }
}

// File-local copies of the base::UmaHistogram* helpers that chrome.histograms
// records through, creating each histogram with `kWebUIHistogramFlags`. Bucket
// layouts match base/metrics/histogram_functions.cc.
void WebUIHistogramExactLinear(const std::string& name,
                               int sample,
                               int exclusive_max) {
  base::LinearHistogram::FactoryGet(name, 1, exclusive_max,
                                    static_cast<size_t>(exclusive_max + 1),
                                    kWebUIHistogramFlags)
      ->Add(sample);
}

void WebUIHistogramCustomCounts(const std::string& name,
                                int sample,
                                int min,
                                int exclusive_max,
                                size_t buckets) {
  base::Histogram::FactoryGet(name, min, exclusive_max, buckets,
                              kWebUIHistogramFlags)
      ->Add(sample);
}

void WebUIHistogramCustomTimes(const std::string& name,
                               base::TimeDelta sample,
                               base::TimeDelta min,
                               base::TimeDelta max,
                               size_t buckets) {
  base::Histogram::FactoryTimeGet(name, min, max, buckets, kWebUIHistogramFlags)
      ->AddTimeMillisecondsGranularity(sample);
}

}  // namespace

void InstallWebUIHistogramsExtension(v8::Isolate* isolate,
                                     v8::Local<v8::Context> context,
                                     v8::Local<v8::Object> chrome) {
  v8::Local<v8::Object> histograms =
      GetOrCreateObject(isolate, context, chrome, "histograms");

  auto bind = [&](const char* name, auto func) {
    histograms
        ->Set(context, gin::StringToSymbol(isolate, name),
              gin::CreateFunctionTemplate(isolate, base::BindRepeating(func))
                  ->GetFunction(context)
                  .ToLocalChecked())
        .Check();
  };

  bind("recordUserAction", [](const std::string& name) {
    content::RenderThread::Get()->RecordComputedAction(name);
  });

  bind("recordBoolean", [](const std::string& name, bool value) {
    // Same layout as base::UmaHistogramBoolean().
    base::BooleanHistogram::FactoryGet(name, kWebUIHistogramFlags)->Add(value);
  });

  bind("recordPercentage", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramPercentage().
    WebUIHistogramExactLinear(name, static_cast<int>(std::round(value)), 101);
  });

  bind("recordSmallCount", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramCounts100().
    WebUIHistogramCustomCounts(name, static_cast<int>(std::round(value)), 1,
                               100, 50);
  });

  bind("recordMediumCount", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramCounts10000().
    WebUIHistogramCustomCounts(name, static_cast<int>(std::round(value)), 1,
                               10000, 50);
  });

  bind("recordCount", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramCounts1M().
    WebUIHistogramCustomCounts(name, static_cast<int>(std::round(value)), 1,
                               1000000, 50);
  });

  bind("recordTime", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramTimes().
    WebUIHistogramCustomTimes(
        name, base::Milliseconds(static_cast<int64_t>(std::round(value))),
        base::Milliseconds(1), base::Seconds(10), 50);
  });

  bind("recordMediumTime", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramMediumTimes().
    WebUIHistogramCustomTimes(
        name, base::Milliseconds(static_cast<int64_t>(std::round(value))),
        base::Milliseconds(1), base::Minutes(3), 50);
  });

  bind("recordLongTime", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramLongTimes().
    WebUIHistogramCustomTimes(
        name, base::Milliseconds(static_cast<int64_t>(std::round(value))),
        base::Milliseconds(1), base::Hours(1), 50);
  });

  bind("recordValue", [](const MetricType& metric, double value) {
    base::HistogramType histogram_type(metric.type == "histogram-linear"
                                           ? base::LINEAR_HISTOGRAM
                                           : base::HISTOGRAM);
    RecordValue(metric.metric_name, histogram_type, metric.min, metric.max,
                metric.buckets, static_cast<int>(std::round(value)));
  });

  bind("recordEnumerationValue",
       [](const std::string& name, double sample, int enum_size) {
         // Same layout as base::UmaHistogramExactLinear().
         WebUIHistogramExactLinear(name, static_cast<int>(std::round(sample)),
                                   enum_size);
       });

  bind("recordSparseValue", [](const std::string& name, double value) {
    // Same layout as base::UmaHistogramSparse().
    base::SparseHistogram::FactoryGet(name, kWebUIHistogramFlags)
        ->Add(static_cast<int>(std::round(value)));
  });
}

}  // namespace content
