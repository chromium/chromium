(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <h3 id='rg1_label'>Lunch Options</h3>
    <ul id='rg1' class='radiogroup' role='radiogroup' aria-labelledby='rg1_label' aria-activedescendant='rg1-r4' tabindex='0' data-dump>
      <li id='rg1-r1' tabindex='-1' role='radio' aria-checked='false'>
        Thai
      </li>
      <li id='rg1-r2' tabindex='-1' role='radio' aria-checked='false'>
        Subway
      </li>
      <li id='rg1-r3' tabindex='-1' role='radio' aria-checked='false'>
        Jimmy Johns
      </li>
      <li id='rg1-r4' tabindex='-1' role='radio' aria-checked='true'>
        Radio Maria
      </li>
      <li id='rg1-r5' tabindex='-1' role='radio' aria-checked='false'>
        Rainbow Gardens
      </li>
    </ul>

   <!-- Start of second Radio Group  -->
    <h3 id='rg2_label'>Drink Options</h3>
    <ul id='rg2' role='radiogroup' aria-labelledby='rg2_label' aria-activedescendant='' tabindex='0' data-dump>
      <li id='rg2-r1' tabindex='-1' role='radio' aria-checked='false'>
        Water
      </li>
      <li id='rg2-r2' tabindex='-1' role='radio' aria-checked='false'>
        Tea
      </li>
      <li id='rg2-r3' tabindex='-1' role='radio' aria-checked='false'>
        Coffee
      </li>
      <li id='rg2-r4' tabindex='-1' role='radio' aria-checked='false'>
        Cola
      </li>
      <li id='rg2-r5' tabindex='-1' role='radio' aria-checked='false'>
        Ginger Ale
      </li>
    </ul>
  `, 'Tests relationship accessibility values.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})
