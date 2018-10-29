self.onmessage = function(e) {
  function ModuleBadAsm() {
    "use asm";
    var x = 1;
    var y = x + 1;
    function foo() {}
    return {bar: foo};
  }
  ModuleBadAsm();
  self.postMessage({name: 'dude'});
};
