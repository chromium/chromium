# UseCounter Wiki

UseCounter measures the usage of HTML and JavaScript features across all
channels and platforms in the wild. Feature usage is recorded per page load and
is anonymously aggregated. Note measurements only take place on HTTP/HTTPS
pages. Usages on new tab page, data URL, or file URL are not. Usages on
extensions is measured on a separate histogram.

UseCounter data can be biased against scenarios where user metrics analysis is
not enabled (e.g., enterprises). However, UseCounter data is essential for
[web compat decision making](https://www.chromium.org/blink/platform-predictability/compat-tools)
and [the blink process for breaking changes](https://sites.google.com/a/chromium.org/dev/blink/removing-features), as it reflects the real Chrome usage with a wide fraction of coverage.
The results are publicly available on https://chromestatus.com/ and internally
(for Google employees) on [UMA dashboard](https://goto.google.com/uma-usecounter)
and [UKM dashboard](https://goto.google.com/ukm-usecounter) with more detailed
break-downs.


## How to Add a UseCounter Feature

UseCounter measures feature usage via UMA histogram and UKM. To add your
feature to UseCounter, simply:
+ Add your feature to the blink::WebFeature enum;
+ Usage can be measured via:
    * MeasureAs=\<enum value\> in the feature's IDL definition; Or
    * blink::UseCounter::Count() for blink side features; Or
    * page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage()
      for browser side features.

Example:
```c++
enum WebFeature {
  ...
  kMyFeature = N,
  ...
}
```
```
interface MyInterface {
  ...
  [MeasureAs=MyFeature] myIdlAttribute;
  ...
}
```
OR
```c++
  MyInterface::MyBlinkSideFunction() {
    ...
    UseCounter::Count(context, WebFeature::MyFeature);
    ...
  }
```
OR
```c++
  MyBrowserSideFunction() {
    ...
    page_load_metrics::MetricsWebContentObserver::RecordFeatureUsage(
      render_frame_host, blink::mojom::WebFeature::MyFeature);
    ...
  }
```

Not all features collect URL-keyed metrics. To opt in your feature to UKM,
simply add your feature to
[UseCounterPageLoadMetricsObserver::GetAllowedUkmFeatures()](https://cs.chromium.org/chromium/src/chrome/browser/page_load_metrics/observers/use_counter/ukm_features.cc)
and get approval from one of the privacy owners.

You can quickly verify that your feature is added to UMA histograms and UKM by
checking chrome://histograms/Blink.UseCounter.Features and chrome://ukm in your
local build.

## Analyze UseCounter Histogram Data

### Public Data on https://chromestatus.com

Usage of JavaScript and HTML features is available
[here](https://chromestatus.com/metrics/feature/popularity).
Usage of CSS properties is available
[here](https://chromestatus.com/metrics/css/popularity).

The data reflects features' daily usage (count of feature hits / count of total
page visits):
+ On all platforms: Android, Windows, Mac, ChromeOS, and Linux.
+ On "official" channels: stable, beta, and dev.
+ For the most dominant milestone.


### UMA Timeline with Formula

Internally (sorry, Google employees only) you can query the usage of a feature
with break-downs on platforms, channels, etc on the
[UMA Timeline dashboard](https://goto.google.com/uma-usecounter).

To create break-downs, select filters on the top of the dashboard, for example,
"Platform", and set the `operation` to "split by". Note that you can also see
data usage within Android Webview by setting a filter on "Platform".

Select Metric:
+ "Blink.UseCounter.Features" for HTML and JavaScript features.
+ "Blink.UseCounter.CSSProperties" for CSS properties.
+ "Blink.UseCounter.AnimatedCSSProperties" for animated CSS properties.
+ "Blink.UseCounter.Extensions.Features" for HTML and JacaScript features on
  extensions.

In the metric panel, select "Formula" in the left-most drop-down. Then Click
"ADD NEW FORMULA" with:
```
"MyFeature" / "PageVisits" * 100
```

This provides timeline data for your feature usage (per page load) with
break-downs, which should more or less reflects the results on chromestatus.com.


### UseCounter Feature in HTTP Archive

HTTP Archive crawls the top 10K sites on the web and records everything from
request and response headers. The data is available on Google BigQuery.

You can find pages that trigger a particular UseCounter using the following
script:

```sql
SELECT
  DATE(yyyymmdd) AS date,
  client AS platform,
  num_url AS url_count,
  pct_urls AS urls_percentile,
  sample_urls AS url
FROM [httparchive:blink_features.usage]
WHERE feature = 'MyFeature'
ORDER BY url_percentile DESC
```
OR

```sql
SELECT
  url
FROM [httparchive:pages.yyyy_mm_dd_mobile]
WHERE
  JSON_EXTRACT(payload, '$._blinkFeatureFirstUsed.Features.MyFeature') IS NOT
  NULL
LIMIT 500
```

You can also find pages that trigger a particular CSS property (during parsing):

```sql
SELECT
  url
FROM [httparchive:pages.yyyy_mm_dd_mobile]
WHERE
  JSON_EXTRACT(payload, '$._blinkFeatureFirstUsed.CSSFeatures.MyCSSProperty')
  IS NOT NULL
LIMIT 500
```

To find pages that trigger a UseCounter and sort by page rank:

```sql
SELECT
  IFNULL(runs.rank, 1000000) AS rank,
  har.url AS url,
FROM [httparchive:latest.pages_desktop] AS har
LEFT JOIN [httparchive:runs.latest_pages] AS runs
  ON har.url = runs.url
WHERE
  JSON_EXTRACT(payload, '$._blinkFeatureFirstUsed.Features.MyFeature') IS NOT
  NULL
ORDER BY rank;
```


### UMA Usage on Fraction of Users
You may also see the fraction of users that trigger your feature at lease once a
day on [UMA Usage dashboard](https://goto.google.com/uma-usecounter-peruser).


## Analyze UseCounter UKM Data
For privacy concerns, UKM data is available for Google employees only.

### UKM Dashboard
UKM dashboard is accessible to Google employees only. Please see [this internal
wiki](https://goto.google.com/usecounter-ukm-wiki) for details.
