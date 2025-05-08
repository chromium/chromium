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
    * `blink::UseCounter::CountWebDXFeature()` or `blink::UseCounter::Count()` for blink side features; Or
    * `content::ContentBrowserClient::LogWeb[DX]FeatureForCurrentPage()` for browser side features.
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

To add a test for a use counter, there are several options:
 + For use counters recorded from the renderer,
   [internal WPTs](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/web_tests/wpt_internal/README.md)
   expose the `internals.isUseCounted` and `internals.clearUseCounter`
   functions for testing use counters recorded for a given document.

 + Chrome browser tests can verify that use counters were recorded by using a
   `base::HistogramTester` to check whether the "Blink.UseCounter.Features"
   histogram was emitted to for the bucket corresponding to the new WebFeature.
   If the use counter is recorded from the renderer,
   `content::FetchHistogramsFromChildProcesses()` can be used to make this
   use counter activity visible to the browser process.

 + For use counters recorded from the browser process (e.g. by calling
   `GetContentClient()->browser()->LogWebFeatureForCurrentPage()`), a content
   browser test can be used by creating a subclass of
   `ContentBrowserTestContentBrowserClient` that implements
   `LogWebFeatureForCurrentPage`, creating an instance of it in
   `SetUpOnMainThread`, and checking whether the instance's
   `LogWebFeatureForCurrentPage` is called. One way to implement this is to
   have `LogWebFeatureForCurrentPage` be defined as a `MOCK_METHOD` and then
   use `EXPECT_CALL` to ensure that the method is called as expected with
   the new WebFeature value. Note that the
   `ContentBrowserTestContentBrowserClient` instance should be destroyed in
   `TearDownOnMainThread`.

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

HTTP Archive crawls the top sites on the web and records everything from
request and response headers. The data is available on Google BigQuery.

You can find usage and sample pages that trigger a particular UseCounter using
the following script:

```sql
SELECT
  date,
  client AS platform,
  num_urls AS url_count,
  pct_urls AS urls_percent,
  sample_urls AS url
FROM `httparchive.blink_features.usage`
WHERE
  feature = 'MyFeature' AND
  date = (SELECT MAX(date) FROM `httparchive.blink_features.usage`)
ORDER BY date DESC
```


Or to see or filter my more data available in the HTTP Archive you can query the main `httparchive.crawl.pages` table like this:

```sql
SELECT DISTINCT
  client,
  page,
  rank
FROM
  `httparchive.crawl.pages`,
  UNNEST (features) As feats
WHERE
  date = '2024-11-01' AND     -- update date to latest month
  feats.feature = 'MyFeature' -- update feature
ORDER BY
  rank
```

### UMA Usage on Fraction of Users
You may also see the fraction of users that trigger your feature at lease once a
day on [UMA Usage dashboard](https://goto.google.com/uma-usecounter-peruser).


## Analyze UseCounter UKM Data
For privacy concerns, UKM data is available for Google employees only.
Please see [this internal
documentation](https://goto.google.com/ukm-blink-usecounter) for details.
