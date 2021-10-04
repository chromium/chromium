// META: global=window,worker
// META: script=/common/get-host-info.sub.js
// META: script=resources/webtransport-test-helpers.sub.js
// META: script=/common/utils.js

// Note: There is no aioquic event for STOP_SENDING yet, so the server does
// not support checking this yet. Hence, tests checking from the STOP_SENDING
// signal cannot be tested yet.

promise_test(async t => {
  const id = token();
  let wt = new WebTransport(webtransport_url(`client-close.py?token=${id}`));
  await wt.ready;

  const bidi_stream = await wt.createBidirectionalStream();

  const writable = bidi_stream.writable;

  const WT_CODE = 139;
  const HTTP_CODE = webtransport_code_to_http_code(WT_CODE);
  await writable.abort(
      new WebTransportError({streamErrorCode: WT_CODE}));

  await wait(10);

  wt = new WebTransport(webtransport_url(`query.py?token=${id}`));
  await wt.ready;

  const streams = await wt.incomingUnidirectionalStreams;
  const streams_reader = streams.getReader();
  const { value: readable }  = await streams_reader.read();
  streams_reader.releaseLock();

  const data = await read_stream_as_json(readable);

  // Check that stream is aborted with RESET_STREAM with the code and reason
  assert_own_property(data, 'stream-close-info');
  const info = data['stream-close-info'];

  assert_equals(info.source, 'reset', 'reset stream');
  assert_equals(info.code, HTTP_CODE, 'code');
}, 'Abort client-created bidirectional stream');

promise_test(async t => {
  const id = token();
  let wt = new WebTransport(webtransport_url(`client-close.py?token=${id}`));
  await wt.ready;

  const stream_reader = wt.incomingBidirectionalStreams.getReader();
  const { value: bidi_stream } = await stream_reader.read();
  stream_reader.releaseLock();

  const writer = bidi_stream.writable.getWriter();

  const WT_CODE = 52;
  const HTTP_CODE = webtransport_code_to_http_code(WT_CODE);
  await writer.abort(
      new WebTransportError({streamErrorCode: WT_CODE}));

  await wait(10);

  wt = new WebTransport(webtransport_url(`query.py?token=${id}`));
  await wt.ready;

  const streams = await wt.incomingUnidirectionalStreams;
  const streams_reader = streams.getReader();
  const { value: readable } = await streams_reader.read();
  streams_reader.releaseLock();

  const data = await read_stream_as_json(readable);

  // Check that stream is aborted with RESET_STREAM with the code and reason
  assert_own_property(data, 'stream-close-info');
  const info = data['stream-close-info'];

  assert_equals(info.source, 'reset', 'reset_stream');
  assert_equals(info.code, HTTP_CODE, 'code');
}, 'Abort server-initiated bidirectional stream');
