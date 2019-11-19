function testMouseSelectionOneDirection(
    start_x, start_y, end_x, end_y,
    expected_start_container, expected_start_offset,
    expected_end_container, expected_end_offset, name) {
  promise_test(() => {
    return new Promise((resolve, reject) => {
      if (!window.chrome || !chrome.gpuBenchmarking)
        return reject();
      window.getSelection().removeAllRanges();
      chrome.gpuBenchmarking.pointerActionSequence(
        [{source: "mouse",
          actions: [
            {name: "pointerDown", x: start_x, y: start_y},
            {name: "pointerMove", x: end_x, y: end_y},
            {name: "pointerUp"},
          ],
        }], resolve);
    }).then(() => {
      var selection = window.getSelection();
      assert_equals(selection.rangeCount, 1, 'rangeCount');
      var range = selection.getRangeAt(0);
      assert_equals(range.startContainer, expected_start_container, 'startContainer');
      assert_equals(range.startOffset, expected_start_offset, 'startOffset');
      assert_equals(range.endContainer, expected_end_container, 'endContainer');
      assert_equals(range.endOffset, expected_end_offset, 'endOffset');
    });
  }, name);
}

function testMouseSelection(
    start_x, start_y, end_x, end_y,
    expected_start_container, expected_start_offset,
    expected_end_container, expected_end_offset, name) {
  testMouseSelectionOneDirection(
      start_x, start_y, end_x, end_y,
      expected_start_container, expected_start_offset,
      expected_end_container, expected_end_offset, name);
  testMouseSelectionOneDirection(
      end_x, end_y, start_x, start_y,
      expected_start_container, expected_start_offset,
      expected_end_container, expected_end_offset,
      name + ' Reversed');
}
