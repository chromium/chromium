// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js
// META: script=/webcodecs/videoFrame-utils.js

// TODO(crbug.com/1231806): Enable this test once direct SAB usage is supported.
// test(t => {
//   testBufferConstructedI420Frame('SharedArrayBuffer');
// }, 'Test SharedArrayBuffer constructed I420 VideoFrame');

test(t => {
  testBufferConstructedI420Frame('Uint8Array(SharedArrayBuffer)');
}, 'Test Uint8Array(SharedArrayBuffer) constructed I420 VideoFrame');
