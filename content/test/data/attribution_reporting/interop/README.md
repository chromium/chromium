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
and optional in "api_config" field.

```jsonc
{
  // Positive integer that controls how many sources can be in the storage per
  // source origin. Formatted as a base-10 string.
  "max_sources_per_origin": "1024",

  // Positive integer that controls the maximum number of distinct destinations
  // covered by pending sources for a given (source site, reporting site).
  // Formatted as a base-10 string.
  "max_destinations_per_source_site_reporting_site": "100",

  // Positive integer that controls the maximum number of distinct destinations
  // covered by sources for a given (source site, reporting site) over
  // a rate limiting window.
  // Formatted as a base-10 string.
  "max_destinations_per_rate_limit_window": "200",

  // Positive integer that controls the total maximum number of distinct
  // destinations covered by sources for a given source site over
  // a rate limiting window.
  // Formatted as a base-10 string.
  "max_destinations_per_rate_limit_window_reporting_site": "50",

  // Positive integer that controls the rate-limiting time window in days for
  // attribution. Formatted as a base-10 string.
  "rate_limit_time_window": "30",

  // Positive integer that controls the maximum number of distinct reporting
  // origins that can register sources for a given (source site, destination site)
  // per rate-limit window. Formatted as a base-10 string.
  "rate_limit_max_source_registration_reporting_origins": "100",

  // Positive integer that controls the maximum number of distinct reporting
  // origins that can create attributions for a given (source site, destination site)
  // per rate-limit window. Formatted as a base-10 string.
  "rate_limit_max_attribution_reporting_origins": "10",

  // Positive integer that controls the maximum number of attributions for a
  // given (source site, destination site, reporting site) per rate-limit window.
  // Formatted as a base-10 string.
  "rate_limit_max_attributions": "100",

  // Positive integer that controls the valid range of trigger data for triggers
  // that are attributed to a navigation source. Formatted as a base-10 string.
  "navigation_source_trigger_data_cardinality": "8",

  // Positive integer that controls the valid range of trigger data for triggers
  // that are attributed to an event source. Formatted as a base-10 string.
  "event_source_trigger_data_cardinality": "2",

  // A string that encodes either "inf" or a double greater than or equal to 0.
  // This controls the randomized response mechanism for event-level reports by
  // adjusting the flip probability.
  "randomized_response_epsilon": "14",

  // Positive integer that controls how many event-level reports can be in the
  // storage per destination. Formatted as a base-10 string.
  "max_event_level_reports_per_destination": "1024",

  // Positive integer that controls how many times a navigation source can create
  // an event-level report. Formatted as a base-10 string.
  "max_attributions_per_navigation_source": "3",

  // Positive integer that controls how many times an event source can create
  // an event-level report. Formatted as a base-10 string.
  "max_attributions_per_event_source": "1",

  // Positive double that controls the max channel capacity in bits for
  // navigation sources.
  "max_navigation_info_gain": "11.46173",

  // Positive double that controls the max channel capacity in bits for event
  // sources.
  "max_event_info_gain": "1.58494",

  // Positive integer that controls how many aggregatable reports can be in the
  // storage per destination. Formatted as a base-10 string.
  "max_aggregatable_reports_per_destination": "1024",

  // Positive integer that controls the maximum sum of the contributions across
  // all buckets per source. Formatted as a base-10 string.
  "aggregatable_budget_per_source": "65536",

  // Non-negative integer that controls the minimum delay in minutes to deliver
  // an aggregatable report. Formatted as a base-10 string.
  "aggregatable_report_min_delay": "10",

  // Non-negative integer that controls the span in minutes to deliver an
  // aggregatable report. Formatted as a base-10 string.
  "aggregatable_report_delay_span": "50",
}
```

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
              "Attribution-Reporting-Register-Source": {
                // Required uint64 formatted as a base-10 string.
                "source_event_id": "123456789",

                // Required site on which the source will be attributed.
                "destination": "https://destination.example",

                // Optional int64 in seconds formatted as a base-10 string.
                // Defaults to 30 days.
                "expiry": "86400",

                // Optional int64 formatted as a base-10 string.
                // Defaults to 0.
                "priority": "-456",

                // Optional dictionary of filters and corresponding values.
                // Defaults to empty.
                "filter_data": {
                  "a": ["b", "c"],
                  "d": []
                },

                // Optional uint64 formatted as a base-10 string. Defaults to
                // null.
                "debug_key": "987",

                // Optional dictionary of aggregation key identifiers and
                // corresponding key pieces.
                "aggregation_keys": {
                  // Value is uint128 formatted as a base-16 string.
                  "a": "0x1"
                },

                // Optional int64 in seconds formatted as a base-10 string.
                // Default to expiry.
                "event_report_window": "86400000",

                // Optional int64 in seconds formatted as a base-10 string.
                // Default to expiry.
                "aggregatable_report_window": "86400000"
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
              "Attribution-Reporting-Register-Trigger": {
                // Optional list of zero or more event trigger data.
                "event_trigger_data": [
                  {
                    // Optional uint64 formatted as a base-10 string.
                    // Defaults to 0.
                    "trigger_data": "3",

                    // Optional int64 formatted as a base-10 string.
                    // Defaults to 0.
                    "priority": "-456",

                    // Optional uint64 formatted as a base-10 string. Defaults to
                    // null.
                    "deduplication_key": "654",

                    // Optional dictionary of filters and corresponding values.
                    // Defaults to empty.
                    "filters": {
                      "a": ["b", "c"],
                      "d": []
                    },

                    // Optional dictionary of negated filters and corresponding
                    // values. Defaults to empty.
                    "not_filters": {
                      "x": ["y"],
                      "z": []
                    }
                  }
                ],

                // Optional list of zero or more aggregatable trigger data.
                "aggregatable_trigger_data": [
                  {
                    // Required uint128 formatted as a base-16 string.
                    "key_piece": "0x10",

                    // Required list of key identifiers.
                    "source_keys": ["a"],

                    // Optional dictionary of filters and corresponding values.
                    // Defaults to empty.
                    "filters": {
                      "a": ["b", "c"],
                      "d": []
                    },

                    // Optional dictionary of negated filters and corresponding
                    // values. Defaults to empty.
                    "not_filters": {
                      "x": ["y"],
                      "z": []
                    }
                  }
                ],

                // Optional dictionary of key identifiers and corresponding
                // values.
                "aggregatable_values": {
                  "a": 123
                },

                // Optional uint64 formatted as a base-10 string. Defaults to
                // null.
                "debug_key": "789",

                // Optional dictionary of filters and corresponding values.
                // Defaults to empty.
                "filters": {
                  "a": ["b", "c"],
                  "d": []
                },

                // Optional list of zero or more aggregatable dedup keys.
                "aggregatable_deduplication_keys": [
                  {
                    // Optional uint64 formatted as a base-10 string. Defaults to
                    // null.
                    "deduplication_key": "654",

                    // Optional dictionary of filters and corresponding values.
                    // Defaults to empty.
                    "filters": {
                      "a": ["b", "c"],
                      "d": []
                    },

                    // Optional dictionary of negated filters and corresponding
                    // values. Defaults to empty.
                    "not_filters": {
                      "x": ["y"],
                      "z": []
                    }
                  }
                ],
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
    ]
  }
}
```
