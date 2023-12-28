// This file contains the actual test code for the two MediaRecorder-ignores-oversize-frames
// tests.

const canvasHeight = 512;

// Generate tests for the given mimeType. maxWidth is the largest allowed width for mimeType. We
// don't test height separately because the checks for frame dimensions are in the encoder
// implementation, not Chrome.
function genTest(mimeType, maxWidth) {
  async_test(t => {
    assert_implements_optional(MediaRecorder.isTypeSupported(mimeType), 'codec not supported');
    const canvas = document.createElement('canvas');
    // Start out oversized.
    canvas.width = maxWidth + 1;
    canvas.height = canvasHeight;

    const context = canvas.getContext('2d');
    context.fillStyle = 'red';
    const stream = canvas.captureStream(0);

    function generateFrame() {
      context.fillRect(0, 0, 10, 10);
      stream.getVideoTracks()[0].requestFrame();
    }

    const recorder = new MediaRecorder(stream, {mimeType});
    // Require a couple empty data in case encoded data isn't processed quickly.
    let dataCount = 0;
    recorder.ondataavailable = t.step_func(event => {
      assert_equals(event.data.size, 0, 'Unexpected data when canvas stream is oversize.');
    });
    recorder.onerror = t.step_func(event => {
      // Error is caused.
      t.done();
    });
    recorder.start();
    t.add_cleanup(() => recorder.stop());
    generateFrame();
    recorder.requestData();
  }, `Causes an error when stream is oversize with codec '${mimeType}'`);
}
