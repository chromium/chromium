# NetLog

This document describes the design and use of logging through NetLog.

[TOC]

## Adding new NetLogging code

Adding information to the NetLog helps debugging. However, logging also requires
careful review as it can impact performance, privacy, and security.

Please add a [net/log/OWNERS](../log/OWNERS) reviewer when adding new NetLog
parameters, or adding information to existing ones.

The high level objectives when adding net logging code are:

* No performance cost when capturing is off.
* Logs captured using [`kDefault`](../log/net_log_capture_mode.h) are safe to
  upload and share publicly.
* Capturing using [`kDefault`](../log/net_log_capture_mode.h) has a low
  performance impact.
* Logs captured using [`kDefault`](../log/net_log_capture_mode.h) are small
  enough to upload to bug reports.
* Events that may emit sensitive information have accompanying unit-tests.
* The event and its possible parameters are documented in
  [net_log_event_type_list.h](../log/net_log_event_type_list.h)

To avoid doing work when logging is off, logging code should generally be
conditional on `NetLog::IsCapturing()`. Note that when specifying parameters
via a lambda, the lambda is already conditional on `IsCapturing()`.

### Binary data and strings

NetLog parameters are specified as a JSON serializable `base::Value`. This has
some subtle implications:

* Do not use `base::Value::Type::STRING` with non-UTF-8 data.
* Do not use `base::Value::Type::BINARY` (the JSON serializer can't handle it)

Instead:

* If the string is likely ASCII or UTF-8, use `NetLogStringValue()`.
* If the string is arbitrary data, use `NetLogBinaryValue()`.
* If the string is guaranteed to be valid UTF-8, you can use
  `base::Value::Type::STRING`

Also consider the maximum size of any string parameters:

* If the string could be large, truncate or omit it when using the default
  capture mode. Large strings should be relegated to the `kEverything`
  capture mode.

### 64-bit integers

NetLog parameters are specified as a JSON serializable `base::Value` which does
not support 64-bit integers.

Be careful when using `base::Value::Dict::Set()` as it will truncate 64-bit
values to 32-bits.

Instead use `NetLogNumberValue()`.

### Backwards compatibility

There is no backwards compatibility requirement for NetLog events and their
parameters, so you are free to change their structure/value as needed.

That said, changing core events may have consequences for external consumers of
NetLogs, which rely on the structure and parameters to events for pretty
printing and log analysis.

The [NetLog viewer](https://netlog-viewer.appspot.com/) for instance pretty
prints certain parameters based on their names, and the event name that added
them.

### Example 1

Add an `PROXY_RESOLUTION_SERVICE` event without any parameters, at all capture
modes.

```
net_log.BeginEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
```

Analysis:

* Privacy: Logging the event at all capture modes only reveals timing
  information.
* Performance: When not logging, has the overhead of an unconditional function
  call (`BeginEvent`), and then a branch (test on `IsCapturing()`).
* Size: Minimal data added to NetLog - just one parameterless event per URL
  request.

### Example 2

Add a `FTP_COMMAND_SENT` event, at all capture modes, along with parameters
that describe the FTP command.

```
if (net_log.IsCapturing()) {
  std::string command = BuildCommandForLog();
  net_log.AddEventWithStringParams(NetLogEventType::FTP_COMMAND_SENT,
                                   "command", command);
}
```

Analysis:

* Privacy: Low risk given FTP traffic is unencrypted. `BuildCommandForString()`
  should additionally best-effort strip any identity information, as this is
  being logged at all capture modes.
* Performance: Costs one branch when not capturing. The call to
  `BuildCommandForString()` is only executed when capturing.
* Size: Cost is proportional to the average FTP command length and frequency of
  FTP, both of which are low. `BuildCommandForLog()` needn't strictly bound the
  string length. If a huge FTP command makes it to a NetLog, there is a good
  chance that is the problem being debugged.

### Example 3

Add a `SSL_CERTIFICATES_RECEIVED` event, along with the full certificate chain,
at all capture modes.

```
net_log.AddEvent(NetLogEventType::SSL_CERTIFICATES_RECEIVED, [&] {
  base::Value::Dict dict;
  base::Value::List certs;
  std::vector<std::string> encoded_chain;
  server_cert_->GetPEMEncodedChain(&encoded_chain);
  for (auto& pem : encoded_chain)
    certs.Append(std::move(pem));
  dict.Set("certificates", std::move(certs));
  return base::Value(std::move(dict));
});
```

Analysis:

* Privacy: Low risk as server certificates are generally public data.
* Performance: Costs one branch when logging is off (hidden by template
  expansion). The code in the lambda which builds the `base::Value` parameters is only
  executed when capturing.
* Size: On average 8K worth of data per request (average of 2K/certificate,
  chain length of 3, and the overhead of PEM-encoding). This is heavy-weight
  for inclusion at `kDefault` capture mode, however justified based on how
  useful the data is.

### Example 4

Add a `COOKIE_STORE_COOKIE_ADDED` event at all capture modes. Moreover, if the
capture mode is `kIncludeSensitive` or `kEverything`, also logs the cookie's
name and value.

```
net_log.AddEvent(NetLogEventType::COOKIE_STORE_COOKIE_ADDED,
                 [&](NetLogCaptureMode capture_mode) {
                   if (!NetLogCaptureIncludesSensitive(capture_mode))
                     return base::Value();
                   base::Value::Dict dict;
                   dict.Set("name", cookie->Name());
                   dict.Set("value", cookie->Value());
                   return base::Value(std::move(dict));
                 });
```

Analysis:

* Privacy: The cookie name and value are not included at the `kDefault` capture
  mode, so only cookie counts and timing information is revealed.
* Performance: Costs one branch when logging is off (hidden by template
  expansion). The code in the lambda which builds the `base::Value` parameters is only
  executed when capturing.
* Size: For default captured logs, has a file size cost proportional to the
  number of cookies added. This is borderline justifiable. It would be better
  in this case to simply omit the event all together at `kDefault` than to log
  a parameterless event, as the parameterless event is not broadly useful.
