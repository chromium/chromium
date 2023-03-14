
async function foo() {
  debugger;
  return 42;
}

const x = await foo();

export {x};
