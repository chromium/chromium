// META: global=window,worker

function runTests(data) {
  for (let entry of data) {
    test(function() {
      const pattern = new URLPattern(entry.pattern);
      assert_equals(pattern.test(entry.input), entry.expected);
    }, `Pattern: ${JSON.stringify(entry.pattern)} Input: ${JSON.stringify(entry.input)}`);
  }
}

promise_test(async function() {
  const response = await fetch('resources/urlpatterntestdata.json');
  const data = await response.json();
  runTests(data);
}, 'Loading data...');
