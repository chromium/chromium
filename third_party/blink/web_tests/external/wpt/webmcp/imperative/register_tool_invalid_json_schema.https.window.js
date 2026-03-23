'use strict';

function test_register_tool_schema_error(inputSchema, expectedError, testName) {
  test(() => {
    assert_throws_js(
      expectedError,
      () => {
        navigator.modelContext.registerTool({
          name: 'empty',
          description: 'empty',
          inputSchema,
          execute: () => {},
        });
      },
      `Should throw ${expectedError.name} for invalid schema`,
    );
  }, testName);
}

test_register_tool_schema_error(
  { toJSON: () => undefined },
  TypeError,
  'registerTool throws when inputSchema.toJSON() returns undefined',
);

test_register_tool_schema_error(
  (() => {
    const circular = {};
    circular.self = circular;
    return circular;
  })(),
  TypeError,
  'registerTool throws when inputSchema contains a circular reference',
);

test_register_tool_schema_error(
  BigInt(42),
  TypeError,
  'registerTool throws when inputSchema contains non-serializable types (BigInt)',
);
