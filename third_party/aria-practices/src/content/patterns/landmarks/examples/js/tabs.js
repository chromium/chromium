/*
 * Copyright 2011-2014 University of Illinois
 * Authors: Thomas Foltz and Jon Gunderson
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* global $ */

'use strict';

$('#myTabs a').click(function (e) {
  e.preventDefault();
  $(this).tab('show');
});
