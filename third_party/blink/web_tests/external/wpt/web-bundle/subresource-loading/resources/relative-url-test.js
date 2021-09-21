// This test tries to load 'script.js' subresource from a static-element.wbn,
// using a relative URL instead of an absolute one with a <link> and <script>
// elements directly in the document (they are used only for this test).
promise_test(async () => {
  assert_equals(resources_script_result, 'loaded from webbundle');
},
'A subresource script.js should be loaded from WebBundle using the relative ' +
'URL.');

// Simple load of a root.js subresource from subresource.wbn using a relative
// URL.
promise_test(async () => {
  const resource_url = '/web-bundle/resources/wbn/root.js';

  const element = createWebBundleElement(
      '../resources/wbn/subresource.wbn',
      [resource_url]);
  document.body.appendChild(element);

  const response = await fetch(resource_url);
  assert_true(response.ok);
  const root = await response.text();
  assert_equals(root, 'export * from \'./submodule.js\';\n');
}, 'Subresources with relative URLs should be loaded from the WebBundle.');

// Simple load of a root.js subresource from subresource.wbn using an
// incorrect relative URL leading to a failed fetch.
promise_test(async () => {
  const resource_url = 'web-bundle/resources/wbn/root.js';

  const element = createWebBundleElement(
    '../resources/wbn/subresource.wbn',
    [resource_url]);
  document.body.appendChild(element);

  const response = await fetch(resource_url);
  assert_false(response.ok);
}, 'Wrong relative URL should result in a failed fetch.');