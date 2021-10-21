
promise_test(async () => {
  const module = await import('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/root.js');
  assert_equals(module.result, 'OK');
}, 'Subresource loading with WebBundle');

promise_test(async () => {
  const response = await fetch('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/root.js');
  const text = await response.text();
  assert_equals(text, 'export * from \'./submodule.js\';\n');
}, 'Subresource loading with WebBundle (Fetch API)');

promise_test(t => {
  const url =
    '/common/redirect.py?location=https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/root.js';
  return promise_rejects_js(t, TypeError, import(url));
}, 'Subresource loading with WebBundle shouldn\'t affect redirect');

promise_test(async () => {
  const element = createWebBundleElement(
      '../resources/wbn/dynamic1-b1.wbn',
      [
        'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource1.js',
        'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource2.js',
        'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource4.js'
      ]);
  document.body.appendChild(element);

  const module = await import('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource1.js');
  assert_equals(module.result, 'resource1 from dynamic1.wbn');
  document.body.removeChild(element);
}, 'Subresource loading from a b1 bundle');

promise_test(async () => {
  const classic_script_url = 'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/classic_script.js';
  const element = createWebBundleElement(
      '../resources/wbn/dynamic1-b1.wbn',
      [classic_script_url]);
  document.body.appendChild(element);
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from dynamic1.wbn');
  changeWebBundleUrl(element, '../resources/wbn/dynamic2-b1.wbn');
  // Loading the classic script should not reuse the previously loaded
  // script. So in this case, the script must be loaded from dynamic2-b1.wbn.
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from dynamic2.wbn');
  document.body.removeChild(element);
  // And in this case, the script must be loaded from network.
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from network');
}, 'Dynamically loading classic script from a \'b1\' web bundle with resources attribute');

promise_test(async () => {
  const element = createWebBundleElement(
      '../resources/wbn/dynamic1.wbn',
      [
        'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource1.js',
        'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource2.js',
        'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource4.js'
      ]);
  document.body.appendChild(element);

  const module = await import('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource1.js');
  assert_equals(module.result, 'resource1 from dynamic1.wbn');

  changeWebBundleUrl(element, '../resources/wbn/dynamic2-b1.wbn');
  const module2 = await import('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource2.js');
  assert_equals(module2.result, 'resource2 from dynamic2.wbn');

  // A resource not specified in the resources attribute, but in the bundle.
  const module3 = await import('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource3.js');
  assert_equals(module3.result, 'resource3 from network');

  document.body.removeChild(element);
  const module4 = await import('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource4.js');
  assert_equals(module4.result, 'resource4 from network');

  // Module scripts are stored to the Document's module map once loaded.
  // So import()ing the same module script will reuse the previously loaded
  // script.
  const module_second = await import('https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource1.js');
  assert_equals(module_second.result, 'resource1 from dynamic1.wbn');
}, 'Dynamically adding / updating / removing the webbundle element.');

promise_test(async () => {
  const classic_script_url = 'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/classic_script.js';
  const element = createWebBundleElement(
      '../resources/wbn/dynamic1.wbn',
      [classic_script_url]);
  document.body.appendChild(element);
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from dynamic1.wbn');
  changeWebBundleUrl(element, '../resources/wbn/dynamic2-b1.wbn');
  // Loading the classic script should not reuse the previously loaded
  // script. So in this case, the script must be loaded from dynamic2.wbn.
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from dynamic2.wbn');
  document.body.removeChild(element);
  // And in this case, the script must be loaded from network.
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from network');
}, 'Dynamically loading classic script from web bundle');

promise_test(async (t) => {
  // To avoid caching mechanism, this test is using fetch() API with
  // { cache: 'no-store' } to load the resource.
  const classic_script_url = 'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/classic_script.js';

  assert_equals(
    await (await fetch(classic_script_url)).text(),
    'window.report_result(\'classic script from network\');\n');

  const element1 = createWebBundleElement(
      '../resources/wbn/dynamic1.wbn',
      [classic_script_url]);
  document.body.appendChild(element1);
  t.add_cleanup(() => {
    if (element1.parentElement)
      element1.parentElement.removeChild(element1);
  });

  assert_equals(
    await (await fetch(classic_script_url, { cache: 'no-store' })).text(),
    'window.report_result(\'classic script from dynamic1.wbn\');\n');

  const element2 = createWebBundleElement(
      '../resources/wbn/dynamic2.wbn',
      [classic_script_url]);
  document.body.appendChild(element2);
  t.add_cleanup(() => {
    if (element2.parentElement)
      element2.parentElement.removeChild(element2);
  });

  assert_equals(
    await (await fetch(classic_script_url, { cache: 'no-store' })).text(),
    'window.report_result(\'classic script from dynamic2.wbn\');\n');

  document.body.removeChild(element2);

  assert_equals(
    await (await fetch(classic_script_url, { cache: 'no-store' })).text(),
    'window.report_result(\'classic script from dynamic1.wbn\');\n');

  document.body.removeChild(element1);

  assert_equals(
    await (await fetch(classic_script_url, { cache: 'no-store' })).text(),
    'window.report_result(\'classic script from network\');\n');
}, 'Multiple web bundle elements. The last added element must be refered.');

promise_test(async () => {
  const classic_script_url = 'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/classic_script.js';
  const scope = 'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/';
  const element = createWebBundleElement(
      '../resources/wbn/dynamic1.wbn',
      [],
      {scopes: [scope]});
  document.body.appendChild(element);
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from dynamic1.wbn');
  changeWebBundleUrl(element, '../resources/wbn/dynamic2.wbn');
  // Loading the classic script should not reuse the previously loaded
  // script. So in this case, the script must be loaded from dynamic2.wbn.
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from dynamic2.wbn');
  // Changes the scope not to hit the classic_script.js.
  changeWebBundleScopes(element, [scope + 'dummy']);
  // And in this case, the script must be loaded from network.
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from network');
  // Adds the scope to hit the classic_script.js.
  changeWebBundleScopes(element, [scope + 'dummy', scope + 'classic_']);
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from dynamic2.wbn');
  document.body.removeChild(element);
  // And in this case, the script must be loaded from network.
  assert_equals(
    await loadScriptAndWaitReport(classic_script_url),
    'classic script from network');
}, 'Dynamically loading classic script from web bundle with scopes');

promise_test(() => {
  return addWebBundleElementAndWaitForLoad(
      '../resources/wbn/dynamic1.wbn?test-event',
      /*resources=*/[],
      {crossOrigin: undefined});
}, 'The webbundle element fires a load event on load success');

promise_test((t) => {
  return addWebBundleElementAndWaitForError(
      '../resources/wbn/nonexistent.wbn',
      /*resources=*/[],
      {crossOrigin: undefined});
}, 'The webbundle element fires an error event on load failure');

promise_test(async () => {
  const module_script_url = 'https://www1.{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource1.js';
  const element = createWebBundleElement(
      '../resources/wbn/dynamic1-crossorigin.wbn',
      [module_script_url]);
  document.body.appendChild(element);
  const module = await import(module_script_url);
  assert_equals(module.result, 'resource1 from network');
}, 'Subresource URL must be same-origin with bundle URL');

promise_test(async () => {
  const module_script_url = 'https://www1.{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/dynamic/resource1.js';
  const element = createWebBundleElement(
      '../resources/wbn/dynamic1-crossorigin-b1.wbn',
      [module_script_url]);
  document.body.appendChild(element);
  const module = await import(module_script_url);
  assert_equals(module.result, 'resource1 from network');
}, 'Subresource URL must be same-origin with bundle URL (for \'b1\' bundles too)');

promise_test(async () => {
  const url = 'urn:uuid:020111b3-437a-4c5c-ae07-adb6bbffb720';
  const element = createWebBundleElement(
      '../resources/wbn/urn-uuid.wbn',
      [url]);
  document.body.appendChild(element);
  assert_equals(await loadScriptAndWaitReport(url), 'OK');
  document.body.removeChild(element);
}, 'Subresource loading with urn:uuid: URL with resources attribute');

promise_test(async () => {
  const url = 'urn:uuid:020111b3-437a-4c5c-ae07-adb6bbffb720';
  const element = createWebBundleElement(
      '../resources/wbn/urn-uuid.wbn',
      [],
      {scopes: ['urn:uuid:']});
  document.body.appendChild(element);
  assert_equals(await loadScriptAndWaitReport(url), 'OK');
  document.body.removeChild(element);
}, 'Subresource loading with urn:uuid: URL with scopes attribute');

promise_test(async () => {
  const url = 'urn:uuid:020111b3-437a-4c5c-ae07-adb6bbffb720';
  const element = createWebBundleElement(
      '../resources/wbn/urn-uuid-b1.wbn',
      [url]);
  document.body.appendChild(element);
  assert_equals(await loadScriptAndWaitReport(url), 'OK');
  document.body.removeChild(element);
}, 'Subresource loading with urn:uuid: URL of a \'b1\' bundle with resources attribute');

promise_test(async () => {
  const url = 'urn:uuid:020111b3-437a-4c5c-ae07-adb6bbffb720';
  const element = createWebBundleElement(
      '../resources/wbn/urn-uuid-b1.wbn',
      [],
      {scopes: ['urn:uuid:']});
  document.body.appendChild(element);
  assert_equals(await loadScriptAndWaitReport(url), 'OK');
  document.body.removeChild(element);
}, 'Subresource loading with urn:uuid: URL of a \'b1\' bundle with scopes attribute');

promise_test(async () => {
  const url = 'uuid-in-package:020111b3-437a-4c5c-ae07-adb6bbffb720';
  const element = createWebBundleElement(
      '../resources/wbn/uuid-in-package.wbn',
      [url]);
  document.body.appendChild(element);
  assert_equals(await loadScriptAndWaitReport(url), 'OK');
  document.body.removeChild(element);
}, 'Subresource loading with uuid-in-package: URL with resources attribute');

promise_test(async () => {
  const url = 'uuid-in-package:020111b3-437a-4c5c-ae07-adb6bbffb720';
  const element = createWebBundleElement(
      '../resources/wbn/uuid-in-package.wbn',
      [],
      {scopes: ['uuid-in-package:']});
  document.body.appendChild(element);
  assert_equals(await loadScriptAndWaitReport(url), 'OK');
  document.body.removeChild(element);
}, 'Subresource loading with uuid-in-package: URL with scopes attribute');


promise_test(async () => {
  const wbn_url = 'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/subresource.wbn?test-resources-update';
  const resource_url = 'https://{{domains[]}}:{{ports[https][0]}}/web-bundle/resources/wbn/submodule.js';
  const element = await addWebBundleElementAndWaitForLoad(wbn_url, /*resources=*/[]);
  changeWebBundleResources(element, [resource_url]);
  const resp = await fetch(resource_url, { cache: 'no-store' });
  assert_true(resp.ok);
  assert_equals(performance.getEntriesByName(wbn_url).length, 1);
  document.body.removeChild(element);
}, 'Updating resource attribute should not reload the bundle');

async function loadScriptAndWaitReport(script_url) {
  const result_promise = new Promise((resolve) => {
    // This function will be called from script.js
    window.report_result = resolve;
  });

  const script = document.createElement('script');
  script.src = script_url;
  document.body.appendChild(script);
  return result_promise;
}
