# URL-Keyed Metrics API

This describes how to write client code to collect UKM data. Before you add new metrics, you should file a proposal.  See [go/ukm](http://go/ukm) for more information.

[TOC]

## Document your metrics in ukm.xml

Any events and metrics you collect need to be documented in [//tools/metrics/ukm/ukm.xml](https://cs.chromium.org/chromium/src/tools/metrics/ukm/ukm.xml)

### Required Details

* Metric `owner`: the email of someone who can answer questions about how this metric is recorded, what it means, and how it should be used. Can include multiple people.
* A `summary` of the event about which you are recording details, including a description of when the event will be recorded.
* For each metric in the event: a `summary` of the data and what it means.
* The `enum` type if the metric is enumerated. The enum uses the [//tools/metrics/histograms/enums.xml](https://cs.chromium.org/chromium/src/tools/metrics/histograms/enums.xml) file for definitions. Note this is the same file for UMA histogram definitions so these can ideally be reused.
* If the metric is numeric then a `unit` should be included.
* If an event will only happen once per Navigation, it can be marked `singular="true"`.

### Example
```xml
<event name="Goat.Teleported">
  <owner>teleporter@chromium.org</owner>
  <summary>
    Recorded when a page teleports a goat.
  </summary>
  <metric name="Duration" unit="ms">
    <summary>
      How long it took to teleport.
    </summary>
  </metric>
  <metric name="GoatType" enum="GoatType">
    <summary>
      The type of goat that was teleported.
    </summary>
  </metric>
</event>
```

### Controlling the Aggregation of Metrics

Control of which metrics are included in the History table is done via the same
[`tools/metrics/ukm/ukm.xml`](https://cs.chromium.org/chromium/src/tools/metrics/ukm/ukm.xml)
file in the Chromium codebase. To have a metric aggregated, `<aggregation>` and
`<history>` tags need to be added.

```xml
<event name="Goat.Teleported">
  <metric name="Duration">
    ...
    <aggregation>
      <history>
        <index fields="profile.country"/>
        <statistics>
          <quantiles type="std-percentiles"/>
        </statistics>
      </history>
    </aggregation>
    ...
  </metric>
</event>
```

Supported statistic types are:

*   `<quantiles type="std-percentiles"/>`: Calculates the "standard percentiles"
    for the values which are 1, 5, 10, 25, 50, 75, 90, 95, and 99%ile.
*   `<enumeration/>`: Calculates the proportions of all values individually. The
    proportions indicate the relative frequency of each bucket and are
    calculated independently for each metric over each aggregation. (Details
    below.)

There can also be one or more `index` tags which define additional aggregation
keys. These are a comma-separated list of keys that is appended to the standard
set. These additional keys are optional but, if present, are always present
together. In other words, "fields=profile.county,profile.form_factor" will cause
all the standard aggregations plus each with *both* country *and* form_factor
but **not** with all the standard aggregations (see above) plus only one of
them. If individual and combined versions are desired, use multiple index tags.

Currently supported additional index fields are:

*   `profile.country`
*   `profile.form_factor`
*   `profile.system_ram`

### Aggregation by Metrics in the Same Event

WARNING: This feature is still under development and isn't ready for use.

### Enumeration Proportions

Proportions are calculated against the number of "page loads" (meaning per
"source" which is usually but not always the same as a browser page load) that
emitted one or more values for the enumeration.  The proportions will sum to 1.0
for an enumeration that emits only one result per page-load if it emits anything
at all. An enumeration emitted more than once per source will result in
proportions that total greater than 1.0 but are still relative to the total
number of loads. If an individual value is emitted more than once per source,
that value's proportion will be greater than 1.0.

For example, `Security.SiteEngagement` emits one value (either 4 or 2) per source:

*   Source ID: 1, URL: https://www.google.com/, enum value: 4
*   Source ID: 2, URL: https://www.facebook.com/, enum value: 2
*   Source ID: 3, URL: https://www.wikipedia.com/, enum value: 4
*   Source ID: 4, URL: https://www.google.com/, enum value: 4

A proportion calculated over all sources would sum to 1.0:

*   2 (0.25)
*   4 (0.75)

In contrast, `VirtualKeyboard.Open:TextInputType` emits multiple values per
source, and can emit the same value multiple times per source. The sum of the
proportions for `VirtualKeyboard.Open:TextInputType` will be greater that 1.0,
and some proportions will be greater than 1.0. In the following example,
`TextInputType` is emitted multiple times per source, and `TextInputType=4` is
emitted more than once per source:

*   Source ID: 1, URL:https://www.google.com/, enum values: [1, 2, 4, 6]
*   Source ID: 2, URL:https://www.facebook.com/, enum values: [2, 4, 5, 4]
*   Source ID: 3, URL:https://www.wikipedia.com/, enum values: [1, 2, 4]

A proportion calculated over all sources would be:

*   1 (0.6667 = 2/3)
*   2 (1.0000 = 3/3)
*   3 (absent)
*   4 (1.3333 = 4/3)
*   5 (0.3333 = 1/3)
*   6 (0.3333 = 1/3)

The denominator for each is 3 because there were 3 sources reporting the metric.
The numerator for each enum value is the count of how many times the value was
emitted.

## Client API

### Get UkmRecorder Instance

In order to record UKM events, your code needs a UkmRecorder object, defined by [//services/metrics/public/cpp/ukm_recorder.h](https://cs.chromium.org/chromium/src/services/metrics/public/cpp/ukm_recorder.h)

There are two main ways of getting a UkmRecorder instance.

1) Use `ukm::UkmRecorder::Get()`.  This currently only works from the Browser process.

2) Use a service connector and get a UkmRecorder.

```cpp
std::unique_ptr<ukm::UkmRecorder> ukm_recorder =
    ukm::UkmRecorder::Create(context()->connector());
ukm::builders::MyEvent(source_id)
    .SetMyMetric(metric_value)
    .Record(ukm_recorder.get());
```

### Get A ukm::SourceId

UKM identifies navigations by their source ID and you'll need to associate an ID with your event in order to tie it to a main frame URL.  Preferably, get an existing ID for the navigation from another object.

The main method for doing this is by getting a navigation ID:

```cpp
ukm::SourceId source_id = GetSourceIdForWebContentsDocument(web_contents);
ukm::SourceId source_id = ukm::ConvertToSourceId(
    navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
```

Some events however occur in the background, and a navigation ID does not exist. In that case, you can use the `ukm::UkmBackgroundRecorderService` to check whether the event can be recorded. This is achieved by using the History Service to determine whether the event's origin is still present in the user profile.

```cpp
// Add the service as a dependency in your service's constructor.
DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());

// Get an instance of the UKM service using a Profile.
auto* ukm_background_service = ukm::UkmBackgroundRecorderFactory::GetForProfile(profile);

// Check if recording a background event for |origin| is allowed.
ukm_background_service->GetBackgroundSourceIdIfAllowed(origin, base::BindOnce(&DidGetBackgroundSourceId));

// A callback will run with an optional source ID.
void DidGetBackgroundSourceId(base::Optional<ukm::SourceId> source_id) {
  if (!source_id) return;  // Can't record as it wasn't found in the history.

  // Use the newly generated source ID.
  ukm::builders::MyEvent(*source_id)
      .SetMyMetric(metric_value)
      .Record(ukm_recorder.get());
}
```

For the remaining cases you may need to temporarily create your own IDs and associate the URL with them. However we currently prefer that this method is not used, and if you need to setup the URL yourself, please email us first at ukm-team@google.com.
Example:

```cpp
ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
ukm_recorder->UpdateSourceURL(source_id, main_frame_url);
```

You will also need to add your class as a friend of UkmRecorder in order to use this private API.

### Create Events

Helper objects for recording your event are generated from the descriptions in ukm.xml.  You can use them like so:

```cpp
#include "services/metrics/public/cpp/ukm_builders.h"

void OnGoatTeleported() {
  ...
  ukm::builders::Goat_Teleported(source_id)
      .SetDuration(duration.InMilliseconds())
      .SetType(goat_type)
      .Record(ukm_recorder);
}
```

If the event name in the XML contains a period (`.`), it is replaced with an underscore (`_`) in the method name.

### Local Testing

Build Chromium and run it with '--force-enable-metrics-reporting --metrics-upload-interval=N'. You may want some small N if you are interested in seeing behavior when UKM reports are emitted. Trigger your event locally and check chrome://ukm to make sure the data was recorded correctly.

## Unit Testing

You can pass your code a TestUkmRecorder (see [//components/ukm/test_ukm_recorder.h](https://cs.chromium.org/chromium/src/components/ukm/test_ukm_recorder.h)) and then use the methods it provides to test that your data records correctly.
