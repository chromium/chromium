class TestAWP extends AudioWorkletProcessor {
  process(_inputs, _outputs, _params) {
    return true;
  }
}

registerProcessor("test-awp", TestAWP);
