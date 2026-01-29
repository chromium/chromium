# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dataclasses
import typing

from robo_lib import shell
from robo_lib import config
from robo_lib.errors import UserInstructions


ExecFN = typing.Callable[[config.RoboConfiguration], None]
SkipFN = typing.Callable[[config.RoboConfiguration], bool]


@dataclasses.dataclass
class Task():
    name: str
    desc: str
    func: ExecFN
    skip: SkipFN = None

    # Tracks all steps without having to load them explicitly.
    _by_name: typing.ClassVar[dict[str, 'Task']] = {}

    # Tracks which steps exist as a substep of another step. This will help
    # with rendering the steps for user selection.
    _subtasks: typing.ClassVar[set[str]] = set()

    # Map from parent to child task nodes.
    _subtask_tree: typing.ClassVar[dict[str, ['Task']]] = {}

    @staticmethod
    def Serial(name:str, desc:str, tasks:['Task'], skip:SkipFN|None=None):
        Task._subtasks.update(set(t.name for t in tasks))
        Task._subtask_tree[name] = tasks
        return Task(name, desc, lambda cfg: RunTasks(cfg, tasks))

    @staticmethod
    def Lookup(name:str) -> 'Task':
        if name not in Task._by_name:
            raise errors.UserInstructions(
                f'`{name}` is not a valid task.\n'
                'run `robo_sushi.py --list` for all valid tasks.')
        return Task._by_name[name]

    def __post_init__(self):
        Task._by_name[self.name] = self

    def can_skip(self, cfg:config.RoboConfiguration) -> bool:
        return self.skip and self.skip(cfg)

    def execute(self, cfg:config.RoboConfiguration) -> None:
        return self.func(cfg)

    def render(self, prefix:str) -> str:
        green = shell.Style.GREEN
        reset = shell.Style.RESET
        return f'{prefix}{green}{self.name}{reset}: {self.desc}'



def RenderTasks(name:str|None=None, prefix:str='') -> None:
    render_prefix = prefix or '• '
    descendent_prefix = prefix or '  '
    if descendent_prefix[-2] == '├':
        descendent_prefix = f'{descendent_prefix[:-2]}│ '
    if descendent_prefix[-2] == '└':
        descendent_prefix = f'{descendent_prefix[:-2]}  '

    if name is not None:
        if task := Task._by_name.get(name, None):
            print(task.render(render_prefix))
            if subtasks := Task._subtask_tree.get(name, None):
                length = len(subtasks)
                for idx, subtask in enumerate(subtasks):
                    if idx+1 == length:
                        RenderTasks(subtask.name, f'{descendent_prefix} └─')
                    else:
                        RenderTasks(subtask.name, f'{descendent_prefix} ├─')
        return
    for task_name in Task._by_name:
        if task_name not in Task._subtasks:
            RenderTasks(task_name)



def MakeErrorStr(queue:[Task]):
    chain = " > ".join([t.name for t in queue])
    return f"\033[31m{chain}\033[0m"


class StepError(Exception):
    def __init__(self, target_queue:[Task], nested_exception:Exception):
        super().__init__(
            f"{MakeErrorStr(target_queue)} "
            + f"{type(nested_exception)}({nested_exception})")
        self._steps = target_queue
        self._nested = nested_exception

    def RaiseFrom(self, target:Task) -> 'StepError':
        raise StepError([target] + self._steps, self._nested) from self._nested


def RunTasks(cfg:config.RoboConfiguration, tasks:[Task]):
    for task in tasks:
        try:
            if cfg.skip_allowed() and task.can_skip(cfg):
                shell.log(f'Skipping Step: `{task.name}`', shell.Style.YELLOW)
            else:
                shell.log(f'Step: `{task.name}`',
                          shell.Style.BLUE + shell.Style.BOLD)
                task.execute(cfg)
        except StepError as se:
            se.RaiseFrom(task)
        except UserInstructions as ui:
            raise ui from None
        except Exception as e:
            raise StepError([task], e) from e
