/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 * ARIA Combobox Examples
 */

'use strict';

var aria = aria || {};

var FRUITS_AND_VEGGIES = [
  ['Apple', 'Fruit'],
  ['Artichoke', 'Vegetable'],
  ['Asparagus', 'Vegetable'],
  ['Banana', 'Fruit'],
  ['Beets', 'Vegetable'],
  ['Bell pepper', 'Vegetable'],
  ['Broccoli', 'Vegetable'],
  ['Brussels sprout', 'Vegetable'],
  ['Cabbage', 'Vegetable'],
  ['Carrot', 'Vegetable'],
  ['Cauliflower', 'Vegetable'],
  ['Celery', 'Vegetable'],
  ['Chard', 'Vegetable'],
  ['Chicory', 'Vegetable'],
  ['Corn', 'Vegetable'],
  ['Cucumber', 'Vegetable'],
  ['Daikon', 'Vegetable'],
  ['Date', 'Fruit'],
  ['Edamame', 'Vegetable'],
  ['Eggplant', 'Vegetable'],
  ['Elderberry', 'Fruit'],
  ['Fennel', 'Vegetable'],
  ['Fig', 'Fruit'],
  ['Garlic', 'Vegetable'],
  ['Grape', 'Fruit'],
  ['Honeydew melon', 'Fruit'],
  ['Iceberg lettuce', 'Vegetable'],
  ['Jerusalem artichoke', 'Vegetable'],
  ['Kale', 'Vegetable'],
  ['Kiwi', 'Fruit'],
  ['Leek', 'Vegetable'],
  ['Lemon', 'Fruit'],
  ['Mango', 'Fruit'],
  ['Mangosteen', 'Fruit'],
  ['Melon', 'Fruit'],
  ['Mushroom', 'Fungus'],
  ['Nectarine', 'Fruit'],
  ['Okra', 'Vegetable'],
  ['Olive', 'Vegetable'],
  ['Onion', 'Vegetable'],
  ['Orange', 'Fruit'],
  ['Parsnip', 'Vegetable'],
  ['Pea', 'Vegetable'],
  ['Pear', 'Fruit'],
  ['Pineapple', 'Fruit'],
  ['Potato', 'Vegetable'],
  ['Pumpkin', 'Fruit'],
  ['Quince', 'Fruit'],
  ['Radish', 'Vegetable'],
  ['Rhubarb', 'Vegetable'],
  ['Shallot', 'Vegetable'],
  ['Spinach', 'Vegetable'],
  ['Squash', 'Vegetable'],
  ['Strawberry', 'Fruit'],
  ['Sweet potato', 'Vegetable'],
  ['Tomato', 'Fruit'],
  ['Turnip', 'Vegetable'],
  ['Ugli fruit', 'Fruit'],
  ['Victoria plum', 'Fruit'],
  ['Watercress', 'Vegetable'],
  ['Watermelon', 'Fruit'],
  ['Yam', 'Vegetable'],
  ['Zucchini', 'Vegetable'],
];

function searchVeggies(searchString) {
  var results = [];

  for (var i = 0; i < FRUITS_AND_VEGGIES.length; i++) {
    var veggie = FRUITS_AND_VEGGIES[i][0].toLowerCase();
    if (veggie.indexOf(searchString.toLowerCase()) === 0) {
      results.push(FRUITS_AND_VEGGIES[i]);
    }
  }

  return results;
}

/**
 * @function onload
 * @description Initialize the combobox examples once the page has loaded
 */
window.addEventListener('load', function () {
  new aria.GridCombobox(
    document.getElementById('ex1-input'),
    document.getElementById('ex1-grid'),
    searchVeggies
  );
});
