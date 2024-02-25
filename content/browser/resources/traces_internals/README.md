# Trace Report UI

This webui implements chrome://traces-internals which shows trace reports
collected by background tracing. To test, simply navigating to the URL might be
sufficient if traces are present. If no traces, chrome can be started with a
trace config to force trace collection:

1. Create a new config.pbtxt file anywhere on your desktop.
Copy the config below and save the file.
```
scenarios: {
  scenario_name: "Test"
  trace_config: {
    data_sources: {
      config: {
        name: "org.chromium.trace_event"
        track_event_config: {
          disabled_categories: ["*"],
          enabled_categories: ["toplevel", "__metadata"]
        },
      }
    }
    data_sources: { config: { name: "org.chromium.trace_metadata" } }
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

2. Then compile the proto with:
```
<outdir>/protoc --encode=perfetto.protos.ChromeFieldTracingConfig \
--proto_path=third_party/perfetto third_party/perfetto/protos/perfetto/config/\
chrome/scenario_config.proto < config.pbtxt > config.pb
```
`config.pbtxt` is the absolute path where your file is on your desktop.
Keep the < > around `config.pbtxt` from the command above.

3. And run chrome with command line:
```
--enable-features=BackgroundTracingDatabase \
--enable-background-tracing=config.pb
```

You should be able to now collect trace reports on your computer and preview
them on chrome://traces-internals.