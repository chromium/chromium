# UseCounter Wiki

UseCounter can measure the usage of HTML, CSS, and JavaScript (etc) features
across all channels and platforms in the wild. Feature usage is recorded per
page load and is anonymously aggregated. Note, measurements are only recorded
for HTTP/HTTPS pages. Usage on the new tab page, on data URLs or file URLs are
not. Usages in extensions is measured on a separate histogram.

UseCounter data can be biased against scenarios where user metrics analysis is
not enabled (e.g., enterprises). However, UseCounter data is essential for
understanding adoption of [new and existing features](https://webstatus.dev/), [web compat decision making](https://www.chromium.org/blink/platform-predictability/compat-tools)
and [the blink process for breaking changes](https://sites.google.com/a/chromium.org/dev/blink/removing-features), as it reflects the real Chrome usage with a wide fraction of coverage.
The results are publicly available on https://chromestatus.com/ and internally
(for Google employees) on [UMA dashboard](https://goto.google.com/uma-usecounter)
and [UKM dashboard](https://goto.google.com/ukm-usecounter) with more detailed
break-downs.


## How to Add a UseCounter Feature

UseCounter measures feature usage via UMA histogram and UKM. To add your
feature to UseCounter, simply:
+ Add your feature to the
  [blink::mojom::WebDXFeature enum](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom)
  or to the [blink::mojom::WebFeature enum](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom);
+ Usage can be recorded via:
    * \[[MeasureAs="WebDXFeature::\<enum value\>"]\] in the feature's IDL definition; Or
    * \[[MeasureAs=\<WebFeature enum value\>]\] in the feature's IDL definition for WebFeature use counters; Or
    * \[[Measure]\] in the feature's IDL definition for WebFeature use counters with an appropriately named use counter; Or
    * blink::UseCounter::CountWebDXFeature() or blink::UseCounter::Count() for blink side features; Or
    * content::ContentBrowserClient::LogWeb[DX]FeatureForCurrentPage() for browser side features.
+ Run [`update_use_counter_feature_enum.py`] to update the UMA mappings.

Example:
```c++
enum WebDXFeature {
  ...
  kMyFeature = N,
  ...
}
```
```
interface MyInterface {
  ...
  [MeasureAs="WebDXFeature::kMyFeature"] myIdlAttribute;
  ...
}
```
OR
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
    UseCounter::CountWebDXFeature(context, WebDXFeature::kMyFeature);
    ...
  }
```
OR
```c++
  MyInterface::MyBlinkSideFunction() {
    ...
    UseCounter::Count(context, WebFeature::kMyFeature);
    ...
  }
```
OR
```c++
  MyBrowserSideFunction() {
    ...
    GetContentClient()->browser()->LogWeb[DX]FeatureForCurrentPage(
      render_frame_host, blink::mojom::Web[DX]Feature::kMyFeature);
    ...
  }
```

All WebDXFeature use counters automatically get URL-keyed metrics collected for
them. But WebFeature and other types of counters do not collect URL-keyed
metrics by default. To opt your non-WebDXFeature feature use counter in to UKM
metrics collection, add your feature to
[UseCounterPageLoadMetricsObserver::GetAllowedUkmFeatures()](https://cs.chromium.org/chromium/src/components/page_load_metrics/browser/observers/use_counter/ukm_features.cc)
and get approval from the privacy/metrics owners.

You can quickly verify that your feature is added to UMA histograms and UKM by
checking chrome://histograms/Blink.UseCounter.WebDXFeatures,
chrome://histograms/Blink.UseCounter.Features and chrome://ukm in your local
build.

## Analyze UseCounter Histogram Data

### Public Data on https://chromestatus.com

Usage of JavaScript and HTML features is publicly available
[here](https://chromestatus.com/metrics/feature/popularity).
Usage of CSS properties is publicly available
[here](https://chromestatus.com/metrics/css/popularity).

The data reflects features' daily usage (count of feature hits / count of total
page visits):
+ On all platforms: Android, Windows, Mac, ChromeOS, and Linux.
+ On "official" channels: stable, beta, and dev.
+ For the most dominant milestone.


### Internal UMA tools

See (https://goto.google.com/uma-usecounter) for internal tooling.

Some metrics of interest:
+ "Blink.UseCounter.WebDXFeatures" for web platform features as defined in the
  [web platform dx repository](https://github.com/web-platform-dx/web-features/).
+ "Blink.UseCounter.Features" for generic Web Platform use counters (some of which are mapped to WebDXFeature use counters).
+ "Blink.UseCounter.CSSProperties" for CSS properties.
+ "Blink.UseCounter.AnimatedCSSProperties" for animated CSS properties.
+ "Blink.UseCounter.Extensions.Features" for HTML and JacaScript features on
  extensions.

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
Please see [this internal
documentation](https://goto.google.com/ukm-blink-usecounter) for details.
