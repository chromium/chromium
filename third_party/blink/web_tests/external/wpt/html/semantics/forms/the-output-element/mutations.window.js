function assert_equal_values(output, value, message) {
  assert_equals(output.value, value, `.value ${message}`);
  assert_equals(output.defaultValue, value, `.defaultValue ${message}`);
}
function assert_values(output, value, defaultValue, message) {
  assert_equals(output.value, value, `.value ${message}`);
  assert_equals(output.defaultValue, defaultValue, `.defaultValue ${message}`);
}

test(() => {
  const output = document.createElement("output"),
        child = output.appendChild(document.createElement("span"));
  assert_equal_values(output, "", "start");
  child.textContent = "x";
  assert_equal_values(output, "x", "after setting textContent");
  output.value = "some";
  assert_values(output, "some", "x", "after setting value");
  output.textContent = "y";
  assert_values(output, "y", "x", "after setting textContent again");
}, "Descendant mutations and output.value and .defaultValue");

test(() => {
  const form = document.createElement("form"),
        output = form.appendChild(document.createElement("output"));
  output.textContent = "value";
  assert_equal_values(output, "value", "after setting textContent");
  output.value = "heya";
  assert_values(output, "heya", "value", "after setting value");
  form.reset();
  assert_equal_values(output, "value", "after form.reset()");

  output.innerHTML = "<div>something</div>";
  assert_equal_values(output, "something", "after setting innerHTML");
  form.reset();
  assert_equal_values(output, "something", "after form.reset() again");
}, "output and output.form.reset()");

test(() => {
  const output = document.createElement('output');
  output.value = '<b>x</b>';
  assert_values(output, '<b>x</b>', '', 'after setting value');
  assert_equals(output.innerHTML, '&lt;b&gt;x&lt;/b&gt;');
  assert_equals(output.childNodes.length, 1);
}, 'output.value setter replaces children with a text node');

test(() => {
  const output = document.createElement('output');
  output.innerHTML = '<b>x</b> y';
  assert_equal_values(output, 'x y', 'after setting innerHTML');
  const child = output.firstChild;
  output.removeChild(child);
  assert_equal_values(output, ' y', 'after removing a child');
}, 'Child list mutations in default mode update output.value and .defaultValue');

test(() => {
  const output = document.createElement('output');
  output.value = 'a';
  output.defaultValue = 'd';
  const child = output.appendChild(document.createElement('span'));
  child.textContent = 'b';
  assert_values(output, 'ab', 'd', 'after appending a child');
  output.removeChild(child);
  assert_values(output, 'a', 'd', 'after removing a child');
  output.innerHTML = '<b>x</b> y';
  assert_values(output, 'x y', 'd', 'after setting innerHTML');
}, 'Child list mutations in value mode update output.value but not .defaultValue');

test(() => {
  const output = document.body.appendChild(document.createElement('output'));
  output.innerHTML =
      '<span class=output-child></span><span class=output-child></span>';
  const list = document.getElementsByClassName('output-child');
  assert_equals(list.length, 2);
  output.firstChild.remove();
  assert_equals(list.length, 1);
  output.remove();
}, 'Live collections are updated after removing a child of output');
