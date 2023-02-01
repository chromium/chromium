JSON schema for the input of the simulator.

```jsonc
{
  // List of zero or more sources to register.
  "sources": [
    {
      // Required time at which to register the source in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "timestamp": "123",

      // Required origin to which the report will be sent if the source is
      // attributed.
      "reporting_origin": "https://reporting.example",

      // Required origin on which to register the source.
      "source_origin": "https://source.example",

      // Required source type, either "navigation" or "event", corresponding to
      // whether the source is registered on click or on view, respectively.
      "source_type": "navigation",

      // Whether the source's reporting origin has debug permission for this
      // event. Defaults to false.
      "debug_permission": true,

      // Required dictionary data to register a source.
      "Attribution-Reporting-Register-Source": {
        // Optional uint64 formatted as a base-10 string. Defaults to 0.
        "source_event_id": "123456789",

        // Required site on which the source will be attributed.
        "destination": "https://destination.example",

        // Optional int64 in seconds formatted as a base-10 string.
        // Default to 30 days.
        "expiry": "86400000",

        // Optional int64 formatted as a base-10 string.
        // Default to 0.
        "priority": "-456",

        // Optional dictionary of filters and corresponding values. Defaults to
        // empty.
        "filter_data": {
          "a": ["b", "c"],
          "d": []
        },

        // Optional uint64 formatted as a base-10 string. Defaults to null.
        "debug_key": "987",

        // Optional dictionary of aggregation key identifiers and corresponding
        // key pieces.
        "aggregation_keys": {
            // Value is uint128 formatted as a base-16 string.
            "a": "0x1"
        },

        // Optional int64 in seconds formatted as a base-10 string.
        // Default to expiry.
        "event_report_window": "86400000",

        // Optional int64 in seconds formatted as a base-10 string.
        // Default to expiry.
        "aggregatable_report_window": "86400000",

        // Optional boolean. Defaults to false.
        "debug_reporting": true
      }
    }
  ],

  // List of zero or more triggers to register.
  "triggers": [
    {
      // Required time at which to register the trigger in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "timestamp": "123",

      // Required origin to which the report will be sent.
      "reporting_origin": "https://reporting.example",

      // Required origin on which the trigger is being registered.
      "destination_origin": "https://destination.example",

      // Whether the trigger's reporting origin has debug permission for this
      // event. Defaults to false.
      "debug_permission": true,

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

            // Optional uint64 formatted as a base-10 string. Defaults to null.
            "deduplication_key": "654",

            // Optional dictionary of filters and corresponding values. Defaults
            // to empty.
            "filters": {
              "a": ["b", "c"],
              "d": []
            },

            // Optional dictionary of negated filters and corresponding values.
            // Defaults to empty.
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

            // Optional dictionary of filters and corresponding values. Defaults
            // to empty.
            "filters": {
              "a": ["b", "c"],
              "d": []
            },

            // Optional dictionary of negated filters and corresponding values.
            // Defaults to empty.
            "not_filters": {
              "x": ["y"],
              "z": []
            }
          }
        ],

        // Optional dictionary of key identifiers and corresponding values.
        "aggregatable_values": {
          "a": 123
        },

        // Optional uint64 formatted as a base-10 string. Defaults to null.
        "debug_key": "789",

        // Optional dictionary of filters and corresponding values. Defaults to
        // empty.
        "filters": {
          "a": ["b", "c"],
          "d": []
        },

        // Optional uint64 formatted as a base-10 string. Defaults to null.
        "aggregatable_deduplication_key": "456",

        // Optional boolean. Defaults to false.
        "debug_reporting": true
      }
    }
  ]
}
```

JSON schema for the output of the simulator.

```jsonc
{
  // List of event-level reports. Omitted if empty.
  "event_level_reports": [
    {
      // Time at which the report would have been sent in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "report_time": "123",

      // URL to which the report would have been sent.
      "report_url": "https://reporting.example/.well-known/attribution-reporting/report-event-attribution",

      // The report itself. See
      // https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#attribution-reports
      // for details about its fields.
      "report": {}
    },
  ],

  // List of debug event-level reports. Omitted if empty.
  "debug_event_level_reports": [],

  // List of aggregatable reports. Omitted if empty.
  "aggregatable_reports": [
    {
      // Time at which the report would have been sent in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "report_time": "123",

      // URL to which the report would have been sent.
      "report_url": "https://reporting.example/.well-known/attribution-reporting/report-aggregate-attribution",

      "test_info": {
        // Aggregatable histograms created from the source and trigger.
        "histograms": [
          {
            // uint128 formatted as a base-16 string.
            "key": "0x0",

            // uint32 value.
            "value": 123
          }
        ],

        // The report itself. See
        // https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATE.md#aggregatable-reports
        // for details about its fields.
        "report": {}
      }
    }
  ],

  // List of debug aggregatable reports. Omitted if empty.
  "debug_aggregatable_reports": [],

  // List of verbose debug reports. Omitted if empty.
  "verbose_debug_reports": [
    {
      // Time at which the report would have been sent in milliseconds since
      // the UNIX epoch formatted as a base-10 string.
      "report_time": "123",

      // URL to which the report would have been sent.
      "report_url": "https://reporting.example/.well-known/attribution-reporting/debug/verbose",

      // The report itself. See
      // https://github.com/WICG/attribution-reporting-api/blob/main/EVENT.md#verbose-debugging-reports
      // for details about its fields.
      "report": {}
    }
  ]
}
```
