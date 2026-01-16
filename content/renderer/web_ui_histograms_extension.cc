// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_histograms_extension.h"

#include <limits.h>

#include <algorithm>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
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
    counter = base::LinearHistogram::FactoryGet(
        name, min, max, buckets,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  } else {
    counter = base::Histogram::FactoryGet(
        name, min, max, buckets,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  }

  // The histogram can be NULL if it is constructed with bad arguments.  Ignore
  // that data for this API.  An error message will be logged.
  if (counter) {
    counter->Add(sample);
  }
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
    base::UmaHistogramBoolean(name, value);
  });

  bind("recordPercentage", [](const std::string& name, int value) {
    base::UmaHistogramPercentage(name, value);
  });

  bind("recordSmallCount", [](const std::string& name, int value) {
    base::UmaHistogramCounts100(name, value);
  });

  bind("recordMediumCount", [](const std::string& name, int value) {
    base::UmaHistogramCounts10000(name, value);
  });

  bind("recordCount", [](const std::string& name, int value) {
    base::UmaHistogramCounts1M(name, value);
  });

  bind("recordTime", [](const std::string& name, int value) {
    base::UmaHistogramTimes(name, base::Milliseconds(value));
  });

  bind("recordMediumTime", [](const std::string& name, int value) {
    base::UmaHistogramMediumTimes(name, base::Milliseconds(value));
  });

  bind("recordLongTime", [](const std::string& name, int value) {
    base::UmaHistogramLongTimes(name, base::Milliseconds(value));
  });

  bind("recordValue", [](const MetricType& metric, int value) {
    base::HistogramType histogram_type(metric.type == "histogram-linear"
                                           ? base::LINEAR_HISTOGRAM
                                           : base::HISTOGRAM);
    RecordValue(metric.metric_name, histogram_type, metric.min, metric.max,
                metric.buckets, value);
  });

  bind("recordEnumerationValue",
       [](const std::string& name, int sample, int enum_size) {
         base::UmaHistogramExactLinear(name, sample, enum_size);
       });

  bind("recordSparseValue", [](const std::string& name, int value) {
    base::UmaHistogramSparse(name, value);
  });
}

}  // namespace content
