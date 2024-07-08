# Interop Tests

This directory contains a set of tests which ensure the attribution logic as
implemented matches the intended behavior of the Attribution Reporting API.

See https://wicg.github.io/attribution-reporting-api/ for the draft specification.

See //content/browser/attribution_reporting/interop/interop_unittest.cc
for the tests.

These tests are purposefully not implemented as web platform tests, so that
they can be shared by non-web-based platforms.

The tests here cover how the browser will handle various series of sources and
triggers with different configurations, but does not rely on any blink APIs.

The vendor-specific parameters can be configured for testing. The default
configuration is specified in `default_config.json` that is contained in this
directory. Each test file can optionally specify the parameters in the
"api_config" field.

# Configuration format

The JSON schema is as follows. All the fields are required in "default_config.json"
and optional in "api_config" field. See the schema in "default_config.json".

# Test case format

The JSON schema is as follows. Timestamps must be distinct across all sources
and triggers.

```jsonc
{
  "description": "description",

  // Optional configuration.
  "api_config": {},

  // Required input.
  "input": {
    // List of zero or more registrations.
    "registrations": [
      {
        // Required time at which to register in milliseconds since
        // the UNIX epoch formatted as a base-10 string.
        "timestamp": "123",

        "registration_request": {
          // Required origin on which to register.
          "context_origin": "https://context.example",

          // A structured dictionary indicating which registrations the
          // responses are eligible for.
          // https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#registration-requests
          "Attribution-Reporting-Eligible": "navigation-source",

          // Whether the request originated from a fenced frame.
          // Defaults to false.
          // https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#verbose-debugging-reports
          "fenced": false
        },

        // List of URLs and the corresponding responses.
        "responses": [
          {
            // Required URL from which the response was sent.
            "url": "https://reporting.example",

            // Whether the registration will be processed with debug permission
            // enabled. Defaults to false.
            "debug_permission": true,

            // Optional for the first response in the list, but required for all
            // subsequent ones. If absent, defaults to the registration's
            // timestamp.
            "timestamp": "456",

            // If present and non-null, the source's randomized response,
            // consisting of zero of more fake reports. Defaults to null. Length
            // must be <= the source's max_event_level_reports. Ignored for
            // triggers.
            "randomized_response": [
              {
                // The fake report's trigger data. Must be a uint32 exactly
                // matching a value in the source's trigger specs.
                "trigger_data": 1,

                // The fake report's report window index. Must be a non-negative
                // integer less than the source's number of report windows.
                "report_window_index": 0
              }
            ],

            // If present and non-null, the lookback days that would create null
            // aggregatable reports. Defaults to null. Ignored for sources.
            "null_aggregatable_reports_days": [1, 5]

            // Exactly one of the registration fields must be present. See
            // https://github.com/WICG/attribution-reporting-api for the
            // complete schema.
            "response": {
              "Attribution-Reporting-Register-Source": { ... },

              "Attribution-Reporting-Register-Trigger": { ... },

              "Attribution-Reporting-Info": "..."
            }
          }
        ]
      }
    ]
  },

  // Expected output.
  "output": {
    "reports": [
      {
        // URL to which the report would have been sent.
        "report_url": "https://reporting.example/.well-known/attribution-reporting/report-event-attribution",

        // Time at which the report would have been sent in milliseconds since
        // the UNIX epoch.
        "report_time": "123",

        "payload": ...
      }
    ]
  }
}
```
