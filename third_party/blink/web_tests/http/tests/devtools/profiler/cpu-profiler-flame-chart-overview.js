// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CPUProfilerTestRunner} from 'cpu_profiler_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests Overview pane calculation in FlameChart for different width = 2^n with n in range 4 - 0.
      Also tests loading of a legacy nodes format, where nodes were represented as a tree.\n`);
  await TestRunner.loadLegacyModule('profiler');

  var profileAndExpectations = {
    profile: {
      'head': {
        'callFrame': {'functionName': '(root)', 'scriptId': '0', 'url': '', 'lineNumber': 0, 'columnNumber': 0},
        'hitCount': 0,
        'callUID': 1017737175,
        'children': [
          {
            'callFrame': {'functionName': '(program)', 'scriptId': '0', 'url': '', 'lineNumber': 0, 'columnNumber': 0},
            'hitCount': 8,
            'callUID': 3228965662,
            'children': [],
            'deoptReason': '',
            'id': 2
          },
          {
            'callFrame': {
              'functionName': 'goog.ui.HoverCard.maybeShow',
              'scriptId': '552',
              'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/hovercard.js',
              'lineNumber': 406,
              'columnNumber': 49
            },
            'hitCount': 0,
            'callUID': 2979117654,
            'children': [{
              'callFrame': {
                'functionName': 'goog.ui.Tooltip.maybeShow',
                'scriptId': '550',
                'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/tooltip.js',
                'lineNumber': 543,
                'columnNumber': 47
              },
              'hitCount': 0,
              'callUID': 4025562866,
              'children': [{
                'callFrame': {
                  'functionName': 'goog.ui.Tooltip.positionAndShow_',
                  'scriptId': '550',
                  'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/tooltip.js',
                  'lineNumber': 610,
                  'columnNumber': 54
                },
                'hitCount': 0,
                'callUID': 3670406757,
                'children': [{
                  'callFrame': {
                    'functionName': 'goog.ui.PopupBase.setVisible',
                    'scriptId': '548',
                    'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                    'lineNumber': 448,
                    'columnNumber': 50
                  },
                  'hitCount': 0,
                  'callUID': 1027332454,
                  'children': [{
                    'callFrame': {
                      'functionName': 'goog.ui.PopupBase.show_',
                      'scriptId': '548',
                      'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                      'lineNumber': 472,
                      'columnNumber': 45
                    },
                    'hitCount': 0,
                    'callUID': 1243771321,
                    'children': [
                      {
                        'callFrame': {
                          'functionName': 'goog.ui.Tooltip.onBeforeShow',
                          'scriptId': '550',
                          'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/tooltip.js',
                          'lineNumber': 462,
                          'columnNumber': 50
                        },
                        'hitCount': 0,
                        'callUID': 730826276,
                        'children': [{
                          'callFrame': {
                            'functionName': 'goog.ui.PopupBase.onBeforeShow',
                            'scriptId': '548',
                            'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                            'lineNumber': 682,
                            'columnNumber': 52
                          },
                          'hitCount': 0,
                          'callUID': 3156514871,
                          'children': [{
                            'callFrame': {
                              'functionName': 'goog.events.EventTarget.dispatchEvent',
                              'scriptId': '537',
                              'url':
                                  'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                              'lineNumber': 185,
                              'columnNumber': 59
                            },
                            'hitCount': 0,
                            'callUID': 175902988,
                            'children': [{
                              'callFrame': {
                                'functionName': 'goog.events.EventTarget.dispatchEventInternal_',
                                'scriptId': '537',
                                'url':
                                    'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                                'lineNumber': 353,
                                'columnNumber': 58
                              },
                              'hitCount': 0,
                              'callUID': 1027992353,
                              'children': [{
                                'callFrame': {
                                  'functionName': 'goog.events.EventTarget.fireListeners',
                                  'scriptId': '537',
                                  'url':
                                      'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                                  'lineNumber': 265,
                                  'columnNumber': 59
                                },
                                'hitCount': 0,
                                'callUID': 3839384186,
                                'children': [{
                                  'callFrame': {
                                    'functionName': 'logEvent',
                                    'scriptId': '554',
                                    'url':
                                        'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/demos\/hovercard.html',
                                    'lineNumber': 120,
                                    'columnNumber': 24
                                  },
                                  'hitCount': 0,
                                  'callUID': 3948503588,
                                  'children': [{
                                    'callFrame': {
                                      'functionName': 'goog.log.info',
                                      'scriptId': '518',
                                      'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/log\/log.js',
                                      'lineNumber': 173,
                                      'columnNumber': 25
                                    },
                                    'hitCount': 0,
                                    'callUID': 2719274562,
                                    'children': [{
                                      'callFrame': {
                                        'functionName': 'goog.debug.DivConsole.addLogRecord',
                                        'scriptId': '517',
                                        'url':
                                            'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/debug\/divconsole.js',
                                        'lineNumber': 92,
                                        'columnNumber': 56
                                      },
                                      'hitCount': 0,
                                      'callUID': 3436172789,
                                      'children': [{
                                        'callFrame': {
                                          'functionName': 'appendChild',
                                          'scriptId': '0',
                                          'url': '',
                                          'lineNumber': 0,
                                          'columnNumber': 0
                                        },
                                        'hitCount': 1,
                                        'callUID': 2375967960,
                                        'children': [],
                                        'deoptReason': '',
                                        'id': 16
                                      }],
                                      'deoptReason': 'no reason',
                                      'id': 15
                                    }],
                                    'deoptReason': 'no reason',
                                    'id': 14
                                  }],
                                  'deoptReason': 'no reason',
                                  'id': 13
                                }],
                                'deoptReason': 'no reason',
                                'id': 12
                              }],
                              'deoptReason': 'no reason',
                              'id': 11
                            }],
                            'deoptReason': 'no reason',
                            'id': 10
                          }],
                          'deoptReason': 'no reason',
                          'id': 9
                        }],
                        'deoptReason': 'no reason',
                        'id': 8
                      },
                      {
                        'callFrame': {
                          'functionName': 'goog.ui.Popup.reposition',
                          'scriptId': '549',
                          'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popup.js',
                          'lineNumber': 199,
                          'columnNumber': 46
                        },
                        'hitCount': 0,
                        'callUID': 994036427,
                        'children': [{
                          'callFrame': {
                            'functionName': 'goog.positioning.AnchoredPosition.reposition',
                            'scriptId': '522',
                            'url':
                                'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/positioning\/anchoredposition.js',
                            'lineNumber': 83,
                            'columnNumber': 66
                          },
                          'hitCount': 0,
                          'callUID': 3611579388,
                          'children': [{
                            'callFrame': {
                              'functionName': 'goog.positioning.positionAtAnchor',
                              'scriptId': '520',
                              'url':
                                  'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/positioning\/positioning.js',
                              'lineNumber': 196,
                              'columnNumber': 45
                            },
                            'hitCount': 0,
                            'callUID': 2475480533,
                            'children': [
                              {
                                'callFrame': {
                                  'functionName': 'goog.positioning.getOffsetParentPageOffset',
                                  'scriptId': '520',
                                  'url':
                                      'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/positioning\/positioning.js',
                                  'lineNumber': 278,
                                  'columnNumber': 54
                                },
                                'hitCount': 0,
                                'callUID': 726686371,
                                'children': [{
                                  'callFrame': {
                                    'functionName': 'get offsetParent',
                                    'scriptId': '0',
                                    'url': '',
                                    'lineNumber': 0,
                                    'columnNumber': 0
                                  },
                                  'hitCount': 1,
                                  'callUID': 165569179,
                                  'children': [],
                                  'deoptReason': '',
                                  'id': 21
                                }],
                                'deoptReason': 'no reason',
                                'id': 20
                              },
                              {
                                'callFrame': {
                                  'functionName': 'goog.positioning.getVisiblePart_',
                                  'scriptId': '520',
                                  'url':
                                      'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/positioning\/positioning.js',
                                  'lineNumber': 316,
                                  'columnNumber': 44
                                },
                                'hitCount': 0,
                                'callUID': 832563978,
                                'children': [{
                                  'callFrame': {
                                    'functionName': 'goog.style.getBounds',
                                    'scriptId': '516',
                                    'url':
                                        'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/style\/style.js',
                                    'lineNumber': 1129,
                                    'columnNumber': 32
                                  },
                                  'hitCount': 0,
                                  'callUID': 1735066671,
                                  'children': [{
                                    'callFrame': {
                                      'functionName': 'goog.style.getPageOffset',
                                      'scriptId': '516',
                                      'url':
                                          'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/style\/style.js',
                                      'lineNumber': 651,
                                      'columnNumber': 36
                                    },
                                    'hitCount': 0,
                                    'callUID': 3913614204,
                                    'children': [{
                                      'callFrame': {
                                        'functionName': 'goog.style.getStyle_',
                                        'scriptId': '516',
                                        'url':
                                            'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/style\/style.js',
                                        'lineNumber': 219,
                                        'columnNumber': 32
                                      },
                                      'hitCount': 0,
                                      'callUID': 1778778665,
                                      'children': [{
                                        'callFrame': {
                                          'functionName': 'get ownerDocument',
                                          'scriptId': '0',
                                          'url': '',
                                          'lineNumber': 0,
                                          'columnNumber': 0
                                        },
                                        'hitCount': 1,
                                        'callUID': 2871300201,
                                        'children': [],
                                        'deoptReason': '',
                                        'id': 26
                                      }],
                                      'deoptReason': 'no reason',
                                      'id': 25
                                    }],
                                    'deoptReason': 'no reason',
                                    'id': 24
                                  }],
                                  'deoptReason': 'no reason',
                                  'id': 23
                                }],
                                'deoptReason': 'no reason',
                                'id': 22
                              }
                            ],
                            'deoptReason': 'no reason',
                            'id': 19
                          }],
                          'deoptReason': 'no reason',
                          'id': 18
                        }],
                        'deoptReason': 'no reason',
                        'id': 17
                      },
                      {
                        'callFrame': {
                          'functionName': 'goog.ui.AdvancedTooltip.onShow_',
                          'scriptId': '551',
                          'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/advancedtooltip.js',
                          'lineNumber': 178,
                          'columnNumber': 53
                        },
                        'hitCount': 0,
                        'callUID': 1438523729,
                        'children': [{
                          'callFrame': {
                            'functionName': 'goog.ui.PopupBase.onShow_',
                            'scriptId': '548',
                            'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                            'lineNumber': 693,
                            'columnNumber': 47
                          },
                          'hitCount': 0,
                          'callUID': 3641174553,
                          'children': [{
                            'callFrame': {
                              'functionName': 'goog.events.EventTarget.dispatchEvent',
                              'scriptId': '537',
                              'url':
                                  'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                              'lineNumber': 185,
                              'columnNumber': 59
                            },
                            'hitCount': 0,
                            'callUID': 175902988,
                            'children': [{
                              'callFrame': {
                                'functionName': 'goog.events.EventTarget.dispatchEventInternal_',
                                'scriptId': '537',
                                'url':
                                    'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                                'lineNumber': 353,
                                'columnNumber': 58
                              },
                              'hitCount': 0,
                              'callUID': 1027992353,
                              'children': [{
                                'callFrame': {
                                  'functionName': 'goog.events.EventTarget.fireListeners',
                                  'scriptId': '537',
                                  'url':
                                      'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                                  'lineNumber': 265,
                                  'columnNumber': 59
                                },
                                'hitCount': 0,
                                'callUID': 3839384186,
                                'children': [{
                                  'callFrame': {
                                    'functionName': 'logEvent',
                                    'scriptId': '554',
                                    'url':
                                        'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/demos\/hovercard.html',
                                    'lineNumber': 120,
                                    'columnNumber': 24
                                  },
                                  'hitCount': 0,
                                  'callUID': 3948503588,
                                  'children': [{
                                    'callFrame': {
                                      'functionName': 'goog.log.info',
                                      'scriptId': '518',
                                      'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/log\/log.js',
                                      'lineNumber': 173,
                                      'columnNumber': 25
                                    },
                                    'hitCount': 0,
                                    'callUID': 2719274562,
                                    'children': [{
                                      'callFrame': {
                                        'functionName': 'goog.debug.DivConsole.addLogRecord',
                                        'scriptId': '517',
                                        'url':
                                            'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/debug\/divconsole.js',
                                        'lineNumber': 92,
                                        'columnNumber': 56
                                      },
                                      'hitCount': 0,
                                      'callUID': 3436172789,
                                      'children': [
                                        {
                                          'callFrame': {
                                            'functionName': 'get scrollHeight',
                                            'scriptId': '0',
                                            'url': '',
                                            'lineNumber': 0,
                                            'columnNumber': 0
                                          },
                                          'hitCount': 1,
                                          'callUID': 1630838451,
                                          'children': [],
                                          'deoptReason': '',
                                          'id': 35
                                        },
                                        {
                                          'callFrame': {
                                            'functionName': 'appendChild',
                                            'scriptId': '0',
                                            'url': '',
                                            'lineNumber': 0,
                                            'columnNumber': 0
                                          },
                                          'hitCount': 1,
                                          'callUID': 2375967960,
                                          'children': [],
                                          'deoptReason': '',
                                          'id': 36
                                        }
                                      ],
                                      'deoptReason': 'no reason',
                                      'id': 34
                                    }],
                                    'deoptReason': 'no reason',
                                    'id': 33
                                  }],
                                  'deoptReason': 'no reason',
                                  'id': 32
                                }],
                                'deoptReason': 'no reason',
                                'id': 31
                              }],
                              'deoptReason': 'no reason',
                              'id': 30
                            }],
                            'deoptReason': 'no reason',
                            'id': 29
                          }],
                          'deoptReason': 'no reason',
                          'id': 28
                        }],
                        'deoptReason': 'no reason',
                        'id': 27
                      }
                    ],
                    'deoptReason': 'TryCatchStatement',
                    'id': 7
                  }],
                  'deoptReason': 'no reason',
                  'id': 6
                }],
                'deoptReason': 'no reason',
                'id': 5
              }],
              'deoptReason': 'no reason',
              'id': 4
            }],
            'deoptReason': 'no reason',
            'id': 3
          },
          {
            'callFrame': {'functionName': '(idle)', 'scriptId': '0', 'url': '', 'lineNumber': 0, 'columnNumber': 0},
            'hitCount': 60,
            'callUID': 316851070,
            'children': [],
            'deoptReason': '',
            'id': 37
          },
          {
            'callFrame': {
              'functionName': 'goog.ui.AdvancedTooltip.maybeHide',
              'scriptId': '551',
              'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/advancedtooltip.js',
              'lineNumber': 272,
              'columnNumber': 55
            },
            'hitCount': 0,
            'callUID': 762635884,
            'children': [{
              'callFrame': {
                'functionName': 'goog.ui.PopupBase.setVisible',
                'scriptId': '548',
                'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                'lineNumber': 448,
                'columnNumber': 50
              },
              'hitCount': 0,
              'callUID': 1027332454,
              'children': [{
                'callFrame': {
                  'functionName': 'goog.ui.PopupBase.hide_',
                  'scriptId': '548',
                  'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                  'lineNumber': 590,
                  'columnNumber': 45
                },
                'hitCount': 0,
                'callUID': 1192193890,
                'children': [
                  {
                    'callFrame': {
                      'functionName': 'goog.ui.PopupBase.onBeforeHide_',
                      'scriptId': '548',
                      'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                      'lineNumber': 708,
                      'columnNumber': 53
                    },
                    'hitCount': 0,
                    'callUID': 3604239577,
                    'children': [{
                      'callFrame': {
                        'functionName': 'goog.events.EventTarget.dispatchEvent',
                        'scriptId': '537',
                        'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                        'lineNumber': 185,
                        'columnNumber': 59
                      },
                      'hitCount': 0,
                      'callUID': 175902988,
                      'children': [{
                        'callFrame': {
                          'functionName': 'goog.events.EventTarget.dispatchEventInternal_',
                          'scriptId': '537',
                          'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                          'lineNumber': 353,
                          'columnNumber': 58
                        },
                        'hitCount': 0,
                        'callUID': 1027992353,
                        'children': [{
                          'callFrame': {
                            'functionName': 'goog.events.EventTarget.fireListeners',
                            'scriptId': '537',
                            'url':
                                'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                            'lineNumber': 265,
                            'columnNumber': 59
                          },
                          'hitCount': 0,
                          'callUID': 3839384186,
                          'children': [{
                            'callFrame': {
                              'functionName': 'logEvent',
                              'scriptId': '554',
                              'url':
                                  'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/demos\/hovercard.html',
                              'lineNumber': 120,
                              'columnNumber': 24
                            },
                            'hitCount': 0,
                            'callUID': 3948503588,
                            'children': [{
                              'callFrame': {
                                'functionName': 'goog.log.info',
                                'scriptId': '518',
                                'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/log\/log.js',
                                'lineNumber': 173,
                                'columnNumber': 25
                              },
                              'hitCount': 0,
                              'callUID': 2719274562,
                              'children': [{
                                'callFrame': {
                                  'functionName': 'goog.debug.DivConsole.addLogRecord',
                                  'scriptId': '517',
                                  'url':
                                      'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/debug\/divconsole.js',
                                  'lineNumber': 92,
                                  'columnNumber': 56
                                },
                                'hitCount': 0,
                                'callUID': 3436172789,
                                'children': [{
                                  'callFrame': {
                                    'functionName': 'set innerHTML',
                                    'scriptId': '0',
                                    'url': '',
                                    'lineNumber': 0,
                                    'columnNumber': 0
                                  },
                                  'hitCount': 1,
                                  'callUID': 707509894,
                                  'children': [],
                                  'deoptReason': '',
                                  'id': 48
                                }],
                                'deoptReason': 'no reason',
                                'id': 47
                              }],
                              'deoptReason': 'no reason',
                              'id': 46
                            }],
                            'deoptReason': 'no reason',
                            'id': 45
                          }],
                          'deoptReason': 'no reason',
                          'id': 44
                        }],
                        'deoptReason': 'no reason',
                        'id': 43
                      }],
                      'deoptReason': 'no reason',
                      'id': 42
                    }],
                    'deoptReason': 'no reason',
                    'id': 41
                  },
                  {
                    'callFrame': {
                      'functionName': 'goog.ui.PopupBase.continueHidingPopup_',
                      'scriptId': '548',
                      'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                      'lineNumber': 627,
                      'columnNumber': 60
                    },
                    'hitCount': 0,
                    'callUID': 2093833058,
                    'children': [{
                      'callFrame': {
                        'functionName': 'goog.ui.HoverCard.onHide_',
                        'scriptId': '552',
                        'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/hovercard.js',
                        'lineNumber': 347,
                        'columnNumber': 47
                      },
                      'hitCount': 0,
                      'callUID': 654814745,
                      'children': [{
                        'callFrame': {
                          'functionName': 'goog.ui.AdvancedTooltip.onHide_',
                          'scriptId': '551',
                          'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/advancedtooltip.js',
                          'lineNumber': 199,
                          'columnNumber': 53
                        },
                        'hitCount': 0,
                        'callUID': 1821323492,
                        'children': [
                          {
                            'callFrame': {
                              'functionName': 'goog.events.unlisten',
                              'scriptId': '535',
                              'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/events.js',
                              'lineNumber': 374,
                              'columnNumber': 32
                            },
                            'hitCount': 2,
                            'callUID': 250969654,
                            'children': [],
                            'deoptReason': 'no reason',
                            'id': 52
                          },
                          {
                            'callFrame': {
                              'functionName': 'goog.ui.Tooltip.onHide_',
                              'scriptId': '550',
                              'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/tooltip.js',
                              'lineNumber': 502,
                              'columnNumber': 45
                            },
                            'hitCount': 0,
                            'callUID': 985322188,
                            'children': [{
                              'callFrame': {
                                'functionName': 'goog.ui.PopupBase.onHide_',
                                'scriptId': '548',
                                'url': 'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/ui\/popupbase.js',
                                'lineNumber': 723,
                                'columnNumber': 47
                              },
                              'hitCount': 0,
                              'callUID': 4290498223,
                              'children': [{
                                'callFrame': {
                                  'functionName': 'goog.events.EventTarget.dispatchEvent',
                                  'scriptId': '537',
                                  'url':
                                      'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                                  'lineNumber': 185,
                                  'columnNumber': 59
                                },
                                'hitCount': 0,
                                'callUID': 175902988,
                                'children': [{
                                  'callFrame': {
                                    'functionName': 'goog.events.EventTarget.dispatchEventInternal_',
                                    'scriptId': '537',
                                    'url':
                                        'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                                    'lineNumber': 353,
                                    'columnNumber': 58
                                  },
                                  'hitCount': 0,
                                  'callUID': 1027992353,
                                  'children': [{
                                    'callFrame': {
                                      'functionName': 'goog.events.EventTarget.fireListeners',
                                      'scriptId': '537',
                                      'url':
                                          'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/events\/eventtarget.js',
                                      'lineNumber': 265,
                                      'columnNumber': 59
                                    },
                                    'hitCount': 0,
                                    'callUID': 3839384186,
                                    'children': [{
                                      'callFrame': {
                                        'functionName': 'logEvent',
                                        'scriptId': '554',
                                        'url':
                                            'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/demos\/hovercard.html',
                                        'lineNumber': 120,
                                        'columnNumber': 24
                                      },
                                      'hitCount': 0,
                                      'callUID': 3948503588,
                                      'children': [{
                                        'callFrame': {
                                          'functionName': 'goog.log.info',
                                          'scriptId': '518',
                                          'url':
                                              'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/log\/log.js',
                                          'lineNumber': 173,
                                          'columnNumber': 25
                                        },
                                        'hitCount': 0,
                                        'callUID': 2719274562,
                                        'children': [{
                                          'callFrame': {
                                            'functionName': 'goog.debug.DivConsole.addLogRecord',
                                            'scriptId': '517',
                                            'url':
                                                'http:\/\/closure-library.googlecode.com\/git\/closure\/goog\/debug\/divconsole.js',
                                            'lineNumber': 92,
                                            'columnNumber': 56
                                          },
                                          'hitCount': 0,
                                          'callUID': 3436172789,
                                          'children': [{
                                            'callFrame': {
                                              'functionName': 'appendChild',
                                              'scriptId': '0',
                                              'url': '',
                                              'lineNumber': 0,
                                              'columnNumber': 0
                                            },
                                            'hitCount': 1,
                                            'callUID': 2375967960,
                                            'children': [],
                                            'deoptReason': '',
                                            'id': 61
                                          }],
                                          'deoptReason': 'no reason',
                                          'id': 60
                                        }],
                                        'deoptReason': 'no reason',
                                        'id': 59
                                      }],
                                      'deoptReason': 'no reason',
                                      'id': 58
                                    }],
                                    'deoptReason': 'no reason',
                                    'id': 57
                                  }],
                                  'deoptReason': 'no reason',
                                  'id': 56
                                }],
                                'deoptReason': 'no reason',
                                'id': 55
                              }],
                              'deoptReason': 'no reason',
                              'id': 54
                            }],
                            'deoptReason': 'no reason',
                            'id': 53
                          }
                        ],
                        'deoptReason': 'no reason',
                        'id': 51
                      }],
                      'deoptReason': 'no reason',
                      'id': 50
                    }],
                    'deoptReason': 'no reason',
                    'id': 49
                  }
                ],
                'deoptReason': 'no reason',
                'id': 40
              }],
              'deoptReason': 'no reason',
              'id': 39
            }],
            'deoptReason': 'no reason',
            'id': 38
          }
        ],
        'deoptReason': '',
        'id': 1
      },
      'startTime': 1384977392.3568,
      'endTime': 1384977392.5345,
      'samples': [
        2,  16, 21, 26, 35, 36, 37, 2,  2,  2,  2,  2,  2,  37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
        37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
        37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 48, 52, 52, 61, 2,  37, 37, 37, 37, 37, 37, 37
      ]
    }
  };
  var profile = profileAndExpectations.profile;
  profile.startTime *= 1000;
  profile.endTime *= 1000;
  var samplingInterval = (profile.endTime - profile.startTime) / (profile.samples.length - 1);
  profile.timeDeltas = [0, ...new Array(profile.samples.length - 1).fill(samplingInterval)];
  profileAndExpectations.cpuProfilerModel = null;
  profileAndExpectations.debuggerModel = null;
  profileAndExpectations.debuggerModel = () => null;
  profileAndExpectations.weakTarget = () => new WeakReference(null);
  profileAndExpectations.profileModel = () => new SDK.CPUProfileDataModel(profile);
  var cpuProfileView = new Profiler.CPUProfileView(profileAndExpectations);
  cpuProfileView.viewSelectComboBox.setSelectedIndex(0);
  cpuProfileView.changeView();
  var overviewPane = cpuProfileView.flameChart.overviewPane;
  console.log(Object.values(overviewPane.calculateDrawData(16)));
  console.log(Object.values(overviewPane.calculateDrawData(8)));
  console.log(Object.values(overviewPane.calculateDrawData(4)));
  console.log(Object.values(overviewPane.calculateDrawData(2)));
  console.log(Object.values(overviewPane.calculateDrawData(1)));
  TestRunner.completeTest();
})();
