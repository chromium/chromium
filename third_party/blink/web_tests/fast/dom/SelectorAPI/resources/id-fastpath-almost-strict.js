setup({single_test: true});



var root = document.createElement('div');
var correctNode = document.createElement('div');
correctNode.setAttribute('id', 'testId');
root.appendChild(correctNode);
document.body.appendChild(root);

assert_equals(document.querySelector('#testId'), correctNode);
assert_equals(document.querySelector('div#testId'), correctNode);
assert_equals(document.querySelector('ul#testId'), null);
assert_equals(document.querySelector('ul #testId'), null);
assert_equals(document.querySelector('#testId[attr]'), null);
assert_equals(document.querySelector('#testId:not(div)'), null);

assert_equals(document.querySelectorAll('div#testId').length, 1);
assert_equals(document.querySelectorAll('div#testId').item(0), correctNode);
assert_equals(document.querySelectorAll('#testId').length, 1);
assert_equals(document.querySelectorAll('#testId').item(0), correctNode);
assert_equals(document.querySelectorAll('ul#testId').length, 0);
assert_equals(document.querySelectorAll('ul #testId').length, 0);
assert_equals(document.querySelectorAll('#testId[attr]').length, 0);
assert_equals(document.querySelectorAll('#testId:not(div)').length, 0);
done();