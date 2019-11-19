(async function() {
  TestRunner.addResult("This tests if the SuggestBox works properly.");

  var delegate = {
      applySuggestion: function(suggestion, isIntermediateSuggestion) {
          if (!suggestion)
              TestRunner.addResult('No item selected.')
          else
              TestRunner.addResult((isIntermediateSuggestion ? "Intermediate " : "") + "Suggestion Applied: " + suggestion.text);
      },
      acceptSuggestion: function() {
          TestRunner.addResult("Suggestion accepted");
      }
  };
  var div = document.createElement("div");
  UI.inspectorView.element.appendChild(div);
  var suggestBox = new UI.SuggestBox(delegate);

  TestRunner.addResult("");
  TestRunner.addResult("Testing that the first item is selected.");
  suggestBox.updateSuggestions(new AnchorBox(50, 50, 400, 400), [
      {text: "First"},
      {text: "Hello"},
      {text: "The best suggestion"}], true, true, "e");

  TestRunner.addResult("");
  TestRunner.addResult("Testing that no item is selected.");
  suggestBox.updateSuggestions(new AnchorBox(50, 50, 400, 400), [
      {text: "First"},
      {text: "Hello", priority: 2},
      {text: "The best suggestion", priority: 5}], false, true, "e");

  TestRunner.addResult("");
  TestRunner.addResult("Testing that highest priority item is selected.");
  suggestBox.updateSuggestions(new AnchorBox(50, 50, 400, 400), [
      {text: "First"},
      {text: "Hello", priority: 2},
      {text: "The best suggestion", priority: 5}], true, true, "e");

  TestRunner.addResult("");
  TestRunner.addResult("Testing that arrow keys can be used for selection.");
  suggestBox.keyPressed(TestRunner.createKeyEvent("ArrowUp"));
  suggestBox.keyPressed(TestRunner.createKeyEvent("ArrowUp"));
  suggestBox.keyPressed(TestRunner.createKeyEvent("ArrowUp"));
  suggestBox.keyPressed(TestRunner.createKeyEvent("ArrowDown"));
  suggestBox.keyPressed(TestRunner.createKeyEvent("ArrowDown"));

  TestRunner.addResult("");
  TestRunner.addResult("Testing that enter can be used to accept a suggestion.");
  suggestBox.keyPressed(TestRunner.createKeyEvent("Enter"));

  TestRunner.addResult("");
  TestRunner.addResult("Testing that highest priority item is selected.");
  suggestBox.updateSuggestions(new AnchorBox(50, 50, 400, 400), [
      {text: "First"},
      {text: "Hello", priority: 2},
      {text: "The best suggestion", priority: 5}], true, true, "e");

  TestRunner.completeTest();
})();
