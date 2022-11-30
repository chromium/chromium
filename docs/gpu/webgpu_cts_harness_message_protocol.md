# WebGPU CTS Harness Message Protocol

The WebGPU conformance test suite harness makes use of a websocket connection to
pass information between the Python and JavaScript code. This document outlines
all valid message types, when they should be sent, etc. All messages are JSON
objects with at least a `type` field that differentiates message types from each
other.

## TEST_STARTED

### Description

A message sent exactly once by the JavaScript code once it starts running the
requested test. In addition to serving as an ack, it also sends information
about how the test will be run.

Sending more than one message of this type during a test is considered an error.

### Fields

* `type` - A string denoting the message type
* `adapter_info` - An optional object containing the same information as the
  [GpuAdapterInfo] the test will be using

[GpuAdapterInfo]: https://gpuweb.github.io/gpuweb/#gpu-adapterinfo

### Example

```
{
  'type': 'TEST_STARTED',
  'adapter_info': {
    'vendor': 'NVIDIA',
    'architecture': 'Turing',
    'device': '2184',
    'description': 'GTX 1660',
  },
}
```

## TEST_HEARTBEAT

### Description

A message sent periodically zero or more times by the JavaScript code to
prevent the Python test harness from timing out.

Sending a message of this type before TEST_STARTED or after TEST_STATUS is
considered an error.

### Fields

* `type` - A string denoting the message type

### Example

```
{
  'type': 'TEST_HEARTBEAT',
}
```

## TEST_STATUS

### Description

A message sent exactly once when the actual test is completed. Contains
information about the status/result of the test.

Sending more than one message of this type or sending before TEST_STARTED is
considered an error.

### Fields

* `type` - As string denoting the message type
* `status` - A string containing the status of the test, e.g. `skip` or `fail`
* `js_duration_ms` - An int containing the number of milliseconds the
  JavaScript code took to run the test

### Example

```
{
  'type': 'TEST_STATUS',
  'status': 'fail',
  'js_duration_ms': 243,
}
```

## TEST_LOG

### Description

A message sent one or more times containing test logs. Multiple messages will be
sent if a single message would exceed the max payload size, in which case the
Python test harness will concatenate them together in the order received.

Sending a message of this type before TEST_STATUS is considered an error.

### Fields

* `type` - A string denoting the message type
* `log` - A string containing log content output by the test

### Example

```
{
  'type': 'TEST_LOG',
  'log': 'Logging is fun',
}
```

## TEST_FINISHED

### Description

A message sent exactly once after all other messages have been sent. This
signals to the Python test harness that it should stop listening for any
additional messages and proceed with test cleanup.

Sending a message of this type before TEST_LOG is considered an error. Sending
more than one message of this type is erroneous, but will not be caught until
the following test is run.

### Fields

* `type` - A string denoting the message type

### Example

```
{
  'type': 'TEST_FINISHED',
}
```
