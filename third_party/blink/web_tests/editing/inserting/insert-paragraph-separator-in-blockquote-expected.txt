This test ensures the paragraph separator inserted between quoted lines inside the blockquote.
You should see a <br> tag between 'First Line' and 'Second Line'.

| <div>
|   id="testDiv"
|   "First Line"
| <div>
|   id="testDiv"
|   <#selection-caret>
|   <br>
| <div>
|   "Second Line"
