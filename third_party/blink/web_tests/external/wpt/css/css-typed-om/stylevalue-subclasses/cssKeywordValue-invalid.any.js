// META: global=window,worker
// META: title=CSSKeywordValue Error Handling

'use strict';

test(() => {
  assert_throws_js(TypeError, () => new CSSKeywordValue(''));
}, 'Constructing CSSKeywordValue with an empty string throws a TypeError');
