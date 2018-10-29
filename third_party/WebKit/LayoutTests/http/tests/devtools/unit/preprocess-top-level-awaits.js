(async function() {
  var commands = [
    '0',
    'await 0',
    'async function foo() { await 0; }',
    'async () => await 0',
    'class A { async method() { await 0 } }',
    'await 0; return 0;',
    'var a = await 1',
    'let a = await 1',
    'const a = await 1',
    'for (var i = 0; i < 1; ++i) { await i }',
    'for (let i = 0; i < 1; ++i) { await i }',
    'var {a} = {a:1}, [b] = [1], {c:{d}} = {c:{d: await 1}}',
    'console.log(`${(await {a:1}).a}`)',
    'await 0;function foo() {}',
    'await 0;class Foo {}',
    'if (await true) { function foo() {} }',
    'if (await true) { class Foo{} }',
    'if (await true) { var a = 1; }',
    'if (await true) { let a = 1; }',
    'var a = await 1; let b = 2; const c = 3;',
    'let o = await 1, p',
    'for await (const number of asyncRandomNumbers()) {}',
    '[...(await fetch(\'url\', { method: \'HEAD\' })).headers.entries()]',
    'await 1\n//hello',
    'var {a = await new Promise(resolve => resolve({a:123}))} = {a : 3}',
    'await 1; for (var a of [1,2,3]);'
  ];

  await TestRunner.loadModule("formatter");
  TestRunner.addResult("This tests preprocessTopLevelAwaitExpressions.");
  for (var command of commands) {
    TestRunner.addResult('--------------');
    TestRunner.addResult(command);
    let processedText = await Formatter.formatterWorkerPool().preprocessTopLevelAwaitExpressions(command);
    if (processedText) {
      TestRunner.addResult('was processed:');
      TestRunner.addResult(processedText);
    } else {
      TestRunner.addResult('was ignored.');
    }
  }
  TestRunner.completeTest();
})();
