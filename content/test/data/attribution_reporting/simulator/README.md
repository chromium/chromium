JSON schema for the input of the simulator.

```json
{
  // List of zero or more sources to register.
  "sources": [
    {
      // Required time at which to register the source in seconds since the
      // UNIX epoch.
      "timestamp": 123,

      // Required origin to which the report will be sent if the source is
      // attributed.
      "reporting_origin": "https://reporting.example",

      // Required origin on which to register the source.
      "source_origin": "https://source.example",

      // Required source type, either "navigation" or "event", corresponding to
      // whether the source is registered on click or on view, respectively.
      "source_type": "navigation",

      // Required dictionary data to register a source.
      "Attribution-Reporting-Register-Source": {
        // Required uint64 formatted as a base-10 string.
        "source_event_id": "123456789",

        // Required site on which the source will be attributed.
        "destination": "https://destination.example",

        // Optional int64 in milliseconds formatted as a base-10 string.
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
        "debug_key": "987"
      },

      // Optional list of zero or more aggregatable keys.
      "Attribution-Reporting-Register-Aggregatable-Source": [
        {
          // Required string to identify the key.
          "id": "a",

          // Required uint128 formatted as a base-16 string.
          "key_piece": "0x1"
        }
      ]
    }
  ],

  // List of zero or more triggers to register.
  "triggers": [
    {
      // Required time at which to register the trigger in seconds since the
      // UNIX epoch.
      "timestamp": 123,

      // Required origin to which the report will be sent.
      "reporting_origin": "https://reporting.example",

      // Required origin on which the trigger is being registered.
      "destination_origin": "https://destination.example",

      // Optional list of zero or more event trigger data.
      "Attribution-Reporting-Register-Event-Trigger": [
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
      "Attribution-Reporting-Register-Aggregatable-Trigger-Data": [
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
      "Attribution-Reporting-Register-Aggregatable-Values": {
        "a": 123
      },

      // Optional uint64 formatted as a base-10 string. Defaults to null.
      "Attribution-Reporting-Trigger-Debug-Key": "789",

      // Optional dictionary of filters and corresponding values. Defaults to
      // empty.
      "Attribution-Reporting-Filters": {
        "a": ["b", "c"],
        "d": []
      }
    }
  ]
}
```
