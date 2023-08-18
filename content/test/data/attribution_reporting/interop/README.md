# Interop Tests

This directory contains a set of tests which ensure the attribution logic as
implemented matches the intended behavior of the Attribution Reporting API.

See https://wicg.github.io/attribution-reporting-api/ for the draft specification.

See //content/browser/attribution_reporting/attribution_interop_unittest.cc
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
    // List of zero or more sources to register.
    "sources": [
      {
        // Required time at which to register the source in milliseconds since
        // the UNIX epoch formatted as a base-10 string.
        "timestamp": "123",

        "registration_request": {
          // Required URL specified in the attributionsrc registration.
          "attribution_src_url": "https://reporting.example",

          // Required origin on which to register the source.
          "source_origin": "https://source.example",

          // Required source type, either "navigation" or "event",
          // corresponding to whether the source is registered on click or
          // view, respectively.
          "source_type": "navigation"
        },

        // List of URLs and the corresponding responses. Currently only allows
        // one.
        "responses": [
          {
            // Required URL from which the response was sent.
            "url": "https://reporting.example",

            // Whether the source will be processed with debug permission
            // enabled. Defaults to false.
            "debug_permission": true,

            "response": {
              // Required dictionary data to register a source.
              // See the explainer https://github.com/WICG/attribution-reporting-api for the complete schema.
              "Attribution-Reporting-Register-Source": {
                ...
              }
            }
          }
        ]
      }
    ],

    // List of zero or more triggers to register.
    "triggers": [
      {
        // Required time at which to register the trigger in milliseconds
        // since the UNIX epoch.
        "timestamp": "123",

        "registration_request": {
          // Required URL from which the response was sent.
          "attribution_src_url": "https://reporting.example",

          // Required origin on which the trigger is being registered.
          "destination_origin": "https://destination.example"
        },

        // List of URLs and the corresponding responses. Currently only allows
        // one.
        "responses": [
          {
            // Required URL from which the response was sent.
            "url": "https://reporting.example",

            // Whether the trigger will be processed with debug permission
            // enabled. Defaults to false.
            "debug_permission": true,

            "response": {
              // See the explainer https://github.com/WICG/attribution-reporting-api for the complete schema.
              "Attribution-Reporting-Register-Trigger": {
                ...
              }
            }
          }
        ]
      }
    ]
  },

  // Expected output.
  "output": {
    // Optional list of event-level results. Omitted if empty.
    "event_level_results": [
      {
        // URL to which the report would have been sent.
        "report_url": "https://reporting.example/.well-known/attribution-reporting/report-event-attribution",

        // Time at which the report would have been sent in milliseconds since
        // the UNIX epoch.
        "report_time": "123",

        "payload": {
          // The attribution destination on which the trigger was registered.
          "attribution_destination": "https://destination.example",

          // uint64 event id formatted as a base-10 string set on the source.
          "source_event_id": "123456789",

          // Coarse uint64 data formatted as a base-10 string set on the
          // trigger.
          "trigger_data": "7",

          // Whether this source was associated with a navigation.
          "source_type": "navigation",

          // Decimal number between 0 and 1 indicating how often noise is
          // applied.
          "randomized_trigger_rate": 0.0024263,

          // Debug key set on the source. Omitted if not set.
          "source_debug_key": "123",

          // Debug key set on the trigger. Omitted if not set.
          "trigger_debug_key": "789"
        }
      }
    ],

    // Optional list of aggregatable results. Omitted if empty.
    "aggregatable_results": [
      {
        // Upper bound time at which the report would have been sent in
        // milliseconds since the UNIX epoch.
        "report_time": "123",

        // URL to which the report would have been sent.
        "report_url": "https://reporting.example/.well-known/attribution-reporting/report-aggregate-attribution",

        "payload": {
          // The attribution destination on which the trigger was registered.
          "attribution_destination": "https://destination.example",

          // List of aggregatable histograms.
          "histograms": [
            {
              // uint128 formatted as a base-16 string.
              "key": "0x1",

              // uint32 value.
              "value": 123
            }
          ],

          // Debug key set on the source. Omitted if not set.
          "source_debug_key": "123",

          // Debug key set on the trigger. Omitted if not set.
          "trigger_debug_key": "789"
        }
      }
    ],

    "verbose_debug_reports": [
      {
        // Upper bound time at which the report would have been sent in
        // milliseconds since the UNIX epoch.
        "report_time": "123",

        // URL to which the report would have been sent.
        "report_url": "https://reporting.example/.well-known/attribution-reporting/debug/verbose"

        // The body of the report.
        "payload": [...]
      }
    ],

    // Optional list of unparsable sources/triggers. Omitted if empty.
    "unparsable_registrations": [
      {
        // Time of the input that failed to register.
        "time": "123",

        "type": "source" // or "trigger"
      }
    ]
  }
}
```
