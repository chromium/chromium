test(() => {
  assert_equals(resources_script_result, 'loaded from webbundle');
},
'A subresource script.js should be loaded from WebBundle using the relative ' +
'URL and a base element.');
