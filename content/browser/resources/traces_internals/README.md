# Traces UI

This WebUI implements `chrome://traces` which contains tools for performance
investigation through tracing.

## Scenarios

`chrome://traces/scenarios` gives control over the tracing scenario configurations.
A Chrome tracing scenarios config allows automatic trace collection by defining a set of scenarios, each associated with specific tracing configs. Scenarios are enrolled based on a set of start and stop rules that delimitate a meaningful tracing interval, usually covering a user journey or a guardian metric (e.g. FirstContentfulPaint).

You can manually enroll in local "preset" scenarios through chrome://traces/scenarios UI.

> **Note**
> If you are here because you'd like to collect a trace for a recurrent performance issue,
> consider enrolling in the "AlwaysOnScenario" scenario.
> Check the descriptions at `chrome://traces/scenarios` to learn more about available scenarios.

On pre-stable channels, you may already be enrolled in scenarios as part of field traces.

You can additionally select a custom scenarios config as a proto (.pb) or base64 encoded (.txt) file; this is meant for someone who wants to test a new config with custom triggers.
If you are a googler, take a look at [go/how-do-i-chrometto#how-do-i-test-background-tracing-setup-locally](go/how-do-i-chrometto#how-do-i-test-background-tracing-setup-locally).

1. Create a new config.pbtxt file anywhere on your desktop.
Copy the example config below and save the file.
```
scenarios: {
  scenario_name: "Test"
  trace_config: {
    data_sources: {
      config: {
        name: "track_event"
        track_event_config: {
          disabled_categories: ["*"],
          enabled_categories: ["toplevel"]
        },
      }
    }
    data_sources: { config: { name: "org.chromium.trace_metadata2" } }
  }
  start_rules: {
    name: "Timer Start"
    delay_ms: 0
  }
  upload_rules: {
    name: "Timer End"
    delay_ms: 5000
  }
}
```

2. Compile the proto with:
```
<outdir>/protoc --encode=perfetto.protos.ChromeFieldTracingConfig \
--proto_path=third_party/perfetto third_party/perfetto/protos/perfetto/config/\
chrome/scenario_config.proto < config.pbtxt > config.pb
```
`config.pbtxt` is the absolute path where your file is on your desktop.
Keep the < > around `config.pbtxt` from the command above.

3. Select the resulting file in `chrome://traces/scenarios`, or run chrome with command line:
```
--enable-background-tracing=config.pb
```

## Reports

When a tracing scenario triggers trace collection, the trace will appear in the
reports list at `chrome://traces`. The .pb trace can be downloaded and opened with
[perfetto](https://ui.perfetto.dev/) or shared with chromium developers. You
can also upload the report and share the trace ID on a crbug.

> **Note** Trace uploads requires enabling privacy filters at `chrome://traces/scenarios`

## Recorder

`chrome://traces/recorder` provides a UI for manual trace collection, either

* Given a trace config as a base64 encoded, passed a `?trace_config=` argument in URL.

> **Note** If you're not sure what config you need, consider simply using the following URL:

```
chrome://traces/recorder?trace_config=CgYIgIAEIAEKBQiAAiACEvYECvMECgt0cmFja19ldmVudIoH4gQKASoSDWFjY2Vzc2liaWxpdHkSBGJhc2USCWJlbmNobWFyaxIFYmxpbmsSCGJsaW5rX2djEgdicm93c2VyEgJjYxIIY2hyb21lb3MSB2NvbnRlbnQSBmRldmljZRIKZGlza19jYWNoZRIGZHdyaXRlEgpleHRlbnNpb25zEgZmbGVkZ2USBWZvbnRzEgNncHUSA2lwYxIMaW50ZXJhY3Rpb25zEgdsYXRlbmN5EgtsYXRlbmN5SW5mbxIHbG9hZGluZxIGbWVtb3J5EgVtb2pvbRIKbW9qb20uZmxvdxIKbmF2aWdhdGlvbhIHb21uaWJveBIJcGFzc3dvcmRzEhVwZXJmb3JtYW5jZV9zY2VuYXJpb3MSH3BlcmZvcm1hbmNlX21hbmFnZXIuY3B1X21ldHJpY3MSCmJhc2UucG93ZXISBXBvd2VyEghyZW5kZXJlchINcmVuZGVyZXJfaG9zdBINU2VydmljZVdvcmtlchIDc3FsEgdzdGFydHVwEgRzeW5jEgh0b3BsZXZlbBINdG9wbGV2ZWwuZmxvdxICdWkSAnY4Egd2OC53YXNtEiBkaXNhYmxlZC1ieS1kZWZhdWx0LWNwdV9wcm9maWxlchIZZGlzYWJsZWQtYnktZGVmYXVsdC1wb3dlchIiZGlzYWJsZWQtYnktZGVmYXVsdC1zeXN0ZW1fbWV0cmljcxInZGlzYWJsZWQtYnktZGVmYXVsdC11c2VyX2FjdGlvbl9zYW1wbGVzEhlkaXNhYmxlZC1ieS1kZWZhdWx0LXY4LmdjEg1zYWZlX2Jyb3dzaW5nEiIKIAocb3JnLmNocm9taXVtLnRyYWNlX21ldGFkYXRhMhABEi8KLQopb3JnLmNocm9taXVtLmJhY2tncm91bmRfc2NlbmFyaW9fbWV0YWRhdGEQARIZChcKFW9yZy5jaHJvbWl1bS50cmlnZ2VycxIfCh0KG29yZy5jaHJvbWl1bS5zeXN0ZW1fbWV0cmljcw%3D%3D
```

* Use selectors UI to craft a custom trace config. The resulting config is
  synced to URL which can be shared.

Once you have a trace config, simply use "Start Tracing" and "Stop Tracing".
Once the tracing session ends, a .pb trace is downloaded, which can be opened with
[perfetto](https://ui.perfetto.dev/) or shared with chromium developers.

## CHANGELOG
M141:
  * Fix scroll in chrome://traces/scenarios
  * chrome://traces/recorder adds UI for ETW data source.

M140:
  * chrome://traces/recorder adds UI for most trace config.

M139:
  * Basic support for chrome://traces/recorder given a trace config in URL
  * Improved UI for chrome://traces/scenarios showing scenarios state.
