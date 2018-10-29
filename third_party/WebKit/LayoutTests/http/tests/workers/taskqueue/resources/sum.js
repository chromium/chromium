class Repeat {
  process(i) { return i; }
}
registerTask("repeat", Repeat);

class Sum {
  process(i, j) { return i + j; }
}
registerTask("sum", Sum);
