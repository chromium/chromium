# 103 Early Hints

Contact: early-hints-experiment@chromium.org

[103 Early Hints](https://httpwg.org/specs/rfc8297.html) is the new HTTP status
code used for preloading subresources earlier. In general, browsers cannot
preload subresources until the main response is served, as resources to be
preloaded are listed on headers or `<meta>` in the main response. Early Hints
will enable browsers to start preloading before the main response is served.
In addition, this can be used with other
[Resource Hints](https://w3c.github.io/resource-hints/) APIs like preconnect.

As of version 87, Chrome doesn't support Early Hints yet, but is going to run an
open experiment to evaluate the effectiveness of Early Hints. This does NOT give
you the real benefit yet, but will help us and the community to quantify the
potential benefit of the feature.

## How To Contribute to the Measurement

Chrome landed code to record the time between when the hints are received to
when the real navigation responses are received as of version 85, and this will
help us learn how much benefit this may bring.

Note that this timing information will be recorded only for the users who
opted-in to help Chrome gather usage statistics. See the
[Google Chrome Privacy Whitepaper](https://www.google.com/chrome/privacy/whitepaper.html#usagestats)
for details.

In order to gather this data, we will need sites to start sending Early Hints
status code (103), so that Chrome can record the timing information for the
hints for the navigation.

Note that not all browsers may handle Early Hints status code gracefully. We are
collaborating with Fastly on the timing to run this measurement, and they
collect breakage reports here: https://early-hints.fastlylabs.com/.

Once enough data is collected, we plan to publish our conclusions and the
learnings from the experiments with the aggregated stats publicly. Upon requests
we may also share the per-site metrics with the sites who have participated.

## Metrics

This section is mainly written for Chromium developers.

Chrome will record the following metrics. These intervals indicate how much
earlier we could start preloading with Early Hints. For example, we could
calculate the ratio of "the interval between request start and response start"
to "the interval between request start and Early Hints" to see the ratio of
speed-up.

### UMA

These are recorded under PageLoad.Experimental.EarlyHints UMA event.

- **FirstRequestStartToEarlyHints**: The interval between when the first HTTP
  request is sent and when the headers of the Early Hints response is received
  in reply to the request for the main resource of a main frame navigation.
- **FinalRequestStartToEarlyHints**: The interval between when the final HTTP
  request is sent and when the headers of the Early Hints response is received
  in reply to the request for the main resource of a main frame navigation.
- **EarlyHintsToFinalResponseStart**: The interval between when the headers of
  the Early Hints response is received in reply to the final HTTP request and
  when the headers of the final HTTP response is received for the main resource
  of a main frame navigation.

### UKM

These are recorded under the NavigationTiming UKM event.

- **EarlyHintsForFirstRequest**: The time relative to navigation start that the
  headers of the Early Hints response are received in reply to the first HTTP
  request for the main resource of a main frame navigation.
- **EarlyHintsForFinalRequest**: The time relative to navigation start that the
  headers of the Early Hints response are received in reply to the final HTTP
  request for the main resource of a main frame navigation.
- **FinalResponseStart**: The time relative to navigation start that the headers
  of the final HTTP response are received for the main resource of a main frame
  navigation.
